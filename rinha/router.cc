#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "glog/logging.h"

#include "thread_pool_router.h"

#define MAX_EVENTS 10
#define BUFFER_SIZE 4096
constexpr char kSocketPath1[] = "/tmp/unix_socket_example.sock";
constexpr char kSocketPath2[] = "/tmp/unix_socket_example2.sock";
constexpr int kMaxConnections = 1000;
constexpr int kInitialUnixConnections = 30;
int socket_to_use = 0;
int tcp_to_unix[kMaxConnections];
int unix_to_tcp[kMaxConnections];

int available_unix_connections[kMaxConnections];
int available_unix_connections_start_idx = 0;
int available_unix_connections_end_idx = 0;

void erase_from_maps(int fd) {
  int unix_fd = tcp_to_unix[fd];
  if (unix_fd != -1) {
    unix_to_tcp[unix_fd] = -1;
    tcp_to_unix[fd] = -1;
    close(fd);
    DLOG(INFO) << "Closed connection on descriptor " << fd << " and "
               << unix_fd;

    available_unix_connections[available_unix_connections_end_idx] = unix_fd;
    available_unix_connections_end_idx =
        (available_unix_connections_end_idx + 1) % kMaxConnections;
    return;
  }

  int tcp_fd = unix_to_tcp[fd];
  if (tcp_fd != -1) {
    tcp_to_unix[tcp_fd] = -1;
    unix_to_tcp[fd] = -1;
    close(tcp_fd);
    close(fd);
    DLOG(INFO) << "Closed connection on descriptor " << fd << " and " << tcp_fd;
    LOG(ERROR) << "Unix closed the connection, shouldn't happen";
    DCHECK(false);
    return;
  }

  DLOG(ERROR) << "Unknown fd";
  DCHECK(false);
}

// Function to set a file descriptor (socket) to non-blocking mode
int make_socket_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    DLOG(ERROR) << "fcntl F_GETFL error: " << strerror(errno);
    return -1;
  }

  flags |= O_NONBLOCK;
  int s = fcntl(fd, F_SETFL, flags);
  if (s == -1) {
    DLOG(ERROR) << "fcntl F_SETFL error: " << strerror(errno);
    return -1;
  }

  return 0;
}

int create_unix_socket_connection() {
  const char *socket_path = socket_to_use % 2 ? kSocketPath1 : kSocketPath2;
  socket_to_use++;
  DLOG(INFO) << "Creating new connection to socket " << socket_path;
  int sock_fd;
  struct sockaddr_un server_addr;

  // Create the UNIX socket
  sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock_fd < 0) {
    LOG(ERROR) << "socket error: " << strerror(errno);
    DCHECK(false);
    return -1;
  }

  // Zero out the server address structure
  memset(&server_addr, 0, sizeof(struct sockaddr_un));

  // Specify the socket family and path
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

  // Connect to the server
  if (connect(sock_fd, (struct sockaddr *)&server_addr,
              sizeof(struct sockaddr_un)) < 0) {
    DLOG(ERROR) << "connect error: " << strerror(errno);
    close(sock_fd); // Close the socket if connection failed
    DCHECK(false);
    return -1;
  }

  DLOG(INFO) << "Created new connection";

  // Return the socket file descriptor
  return sock_fd;
}

int create_unix_connection_and_add_to_epoll(int epoll_fd) {
  // Create a new connection to the unix socket
  int unix_fd = create_unix_socket_connection();
  if (unix_fd == -1) {
    DLOG(ERROR) << "Failed to create new connection to unix socket";
    close(unix_fd);
    DCHECK(false);
    return -1;
  }

  // Make the outgoing socket non-blocking
  int success = make_socket_non_blocking(unix_fd);
  if (success == -1) {
    close(unix_fd);
    DCHECK(false);
    return -1;
  }

  // Add the new socket to the epoll
  static thread_local struct epoll_event event;
  event.data.fd = unix_fd;
  event.events = EPOLLIN | EPOLLET;
  success = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, unix_fd, &event);
  if (success == -1) {
    LOG(ERROR) << "epoll_ctl" << strerror(errno);
    close(unix_fd);
    DCHECK(false);
    return -1;
  }
  return unix_fd;
}

// Function to create a Unix socket and bind it to a specified path
int create_and_bind() {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    LOG(ERROR) << "socket error: " << strerror(errno);
    CHECK(false);
    return false;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(9999);
  if (inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr) <= 0) {
    LOG(ERROR) << "inet_pton error: " << strerror(errno);
    close(sfd);
    CHECK(false);
    return false;
  }

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
    LOG(ERROR) << "bind error: " << strerror(errno);
    close(sfd);
    CHECK(false);
    return false;
  }

  return sfd;
}

int main(int argc, char *argv[]) {
  DLOG(INFO) << "Starting revese proxy";
  absl::ParseCommandLine(argc, argv);
  std::fill_n(tcp_to_unix, kMaxConnections, -1);
  std::fill_n(unix_to_tcp, kMaxConnections, -1);
  std::fill_n(available_unix_connections, kMaxConnections, -1);

  int server_fd, success;
  int epoll_fd;

  struct epoll_event event;
  struct epoll_event *events;

  server_fd = create_and_bind();
  if (server_fd == -1) {
    abort();
  }

  success = make_socket_non_blocking(server_fd);
  if (success == -1) {
    abort();
  }

  success = listen(server_fd, SOMAXCONN);
  if (success == -1) {
    LOG(ERROR) << "listen error: " << strerror(errno);
    abort();
  }

  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    LOG(ERROR) << "epoll_create error: " << strerror(errno);
    abort();
  }

  event.data.fd = server_fd;
  event.events = EPOLLIN | EPOLLET; // Read operation | Edge-triggered
  success = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
  if (success == -1) {
    LOG(ERROR) << "epoll_ctl error: " << strerror(errno);
    abort();
  }

  // Buffer where events are returned
  events = (epoll_event *)calloc(MAX_EVENTS, sizeof(event));

  rinha::InitializeThreadPool(10);

  CHECK(available_unix_connections_end_idx < kMaxConnections);
  // Initialize a few unix connections
  for (int i = 0; i < kInitialUnixConnections; i++) {
    int unix_fd = create_unix_connection_and_add_to_epoll(epoll_fd);

    available_unix_connections[available_unix_connections_end_idx] = unix_fd;
    available_unix_connections_end_idx++;
  }

  LOG(INFO) << "Ready to receive connections...";
  // The event loop
  while (true) {
    int n, i;

    n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    for (i = 0; i < n; i++) {
      if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
          (!(events[i].events & EPOLLIN))) {
        // An error has occured on this fd, or the socket is not
        // ready for reading (why were we notified then?)
        LOG(ERROR) << "epoll error: " << strerror(errno);
        erase_from_maps(events[i].data.fd);
        continue;
      } else if (server_fd == events[i].data.fd) {
        // We have a notification on the listening socket, which
        // means one or more incoming connections.
        while (true) {
          struct sockaddr in_addr;
          socklen_t in_len;
          int infd;

          in_len = sizeof(in_addr);
          infd = accept(server_fd, &in_addr, &in_len);
          if (infd == -1) {
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
              // We have processed all incoming
              // connections.
              break;
            }
            // else {
            //   DLOG(INFO) << "accepting new connection";
            //   break;
            // }
          }

          // Make the incoming socket non-blocking and add it to the
          // list of fds to monitor.
          success = make_socket_non_blocking(infd);
          if (success == -1)
            abort();

          event.data.fd = infd;
          event.events = EPOLLIN | EPOLLET;
          success = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event);
          if (success == -1) {
            LOG(ERROR) << "epoll_ctl" << strerror(errno);
            abort();
          }

          int unix_fd = -1;
          // Check if we can resuse an existing unix connection
          if (available_unix_connections_start_idx !=
              available_unix_connections_end_idx) {
            unix_fd = available_unix_connections
                [available_unix_connections_start_idx];
            available_unix_connections_start_idx =
                (available_unix_connections_start_idx + 1) % kMaxConnections;
          } else {
            // Create a new connection to the unix socket
            unix_fd = create_unix_connection_and_add_to_epoll(epoll_fd);
          }

          // Add the new file descriptors to the maps
          DLOG(INFO) << "Adding new file descriptors to the maps: " << infd
                     << " and " << unix_fd;
          tcp_to_unix[infd] = unix_fd;
          unix_to_tcp[unix_fd] = infd;
        }
        continue;
      } else {
        // We have data on the fd waiting to be read. Read and
        // display it. We must read whatever data is available
        // completely, as we are running in edge-triggered mode
        // and won't get a notification again for the same
        // data.
        int src_fd = events[i].data.fd;
        int bytes_available;
        if (ioctl(src_fd, FIONREAD, &bytes_available) == -1) {
          LOG(ERROR) << "ioctl error: " << strerror(errno);
          DCHECK(false);
          return 1;
        }
        if (bytes_available == 0) {
          // Closing the descriptor will make epoll remove it
          // from the set of descriptors which are monitored.
          erase_from_maps(events[i].data.fd);
        } else {
          int dst_fd = tcp_to_unix[src_fd];
          if (dst_fd == -1) {
            dst_fd = unix_to_tcp[src_fd];
          }
          DCHECK(dst_fd != -1);
          rinha::EnqueueProcessRequest(rinha::Message{src_fd, dst_fd});
        }
      }
    }
  }

  free(events);
  close(server_fd);

  return EXIT_SUCCESS;
}
