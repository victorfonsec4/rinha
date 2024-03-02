#include <arpa/inet.h>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sys/epoll.h>
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
constexpr char kSocketPath[] = "/tmp/unix_socket_example.sock";
absl::flat_hash_map<int, int> tcp_to_unix;
absl::flat_hash_map<int, int> unix_to_tcp;

void erase_from_maps(int fd) {
  auto it = tcp_to_unix.find(fd);
  if (it != tcp_to_unix.end()) {
    close(it->first);
    close(it->second);
    unix_to_tcp.erase(it->second);
    tcp_to_unix.erase(it);
    return;
  }

  it = unix_to_tcp.find(fd);
  if (it != unix_to_tcp.end()) {
    close(it->first);
    close(it->second);
    tcp_to_unix.erase(it->second);
    unix_to_tcp.erase(it);
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
  DLOG(INFO) << "Creating new connection to socket " << kSocketPath;
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
  strncpy(server_addr.sun_path, kSocketPath, sizeof(server_addr.sun_path) - 1);

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

          // Create a new connection to the unix socket
          int unix_fd = create_unix_socket_connection();
          if (unix_fd == -1) {
            DLOG(ERROR) << "Failed to create new connection to unix socket";
            close(infd);
            DCHECK(false);
            continue;
          }

          // Make the outgoing socket non-blocking
          success = make_socket_non_blocking(unix_fd);
          if (success == -1) {
            close(infd);
            close(unix_fd);
            DCHECK(false);
            abort();
          }

          // Add the new socket to the epoll
          event.data.fd = unix_fd;
          event.events = EPOLLIN | EPOLLET;
          success = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, unix_fd, &event);
          if (success == -1) {
            LOG(ERROR) << "epoll_ctl" << strerror(errno);
            close(infd);
            close(unix_fd);
            abort();
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
        static thread_local char buffers[3000][BUFFER_SIZE];
        static thread_local int buffer_index = 0;

        char *buf = buffers[buffer_index++ % 3000];
        int current_buffer_idx = 0;

        bool closed_connection = false;
        ssize_t total_count = 0;
        while (true) {
          ssize_t count;

          DLOG(INFO) << "Reading from client: " << events[i].data.fd;
          count = read(events[i].data.fd, buf + total_count,
                       BUFFER_SIZE - total_count);
          DLOG(INFO) << "Read " << count << " bytes";
          if (count == -1) {
            // If errno == EAGAIN, that means we have read all
            // data. So go back to the main loop.
            if (errno != EAGAIN) {
              LOG(ERROR) << "read error : " << strerror(errno);
              closed_connection = true;
            }
            break;
          } else if (count == 0) {
            // End of file. The remote has closed the
            // connection.
            closed_connection = true;
            break;
          }
          if (count > 0) {
            total_count += count;
          }
        }

        if (total_count > 0) {
          // Get the corresponding file descriptor
          int other_fd;
          auto it = tcp_to_unix.find(events[i].data.fd);
          if (it != tcp_to_unix.end()) {
            other_fd = it->second;
          } else {
            it = unix_to_tcp.find(events[i].data.fd);
            if (it != unix_to_tcp.end()) {
              other_fd = it->second;
            } else {
              DLOG(ERROR) << "Unknown fd";
              DCHECK(false);
            }
          }

          DLOG(INFO) << "Fd association is " << events[i].data.fd << " -> "
                     << other_fd;

          rinha::Message m;
          m.fd = other_fd;
          m.buffer = buf;
          m.size = total_count;

          rinha::EnqueueProcessRequest(std::move(m));
          DLOG(INFO) << "Received message from fd " << events[i].data.fd
                     << " with size " << total_count << " bytes";
        }
        if (closed_connection) {
          DLOG(INFO) << "Closed connection on descriptor " << events[i].data.fd;

          // Closing the descriptor will make epoll remove it
          // from the set of descriptors which are monitored.
          erase_from_maps(events[i].data.fd);
        }
      }
    }
  }

  free(events);
  close(server_fd);

  return EXIT_SUCCESS;
}
