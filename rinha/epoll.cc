#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "glog/logging.h"

#include "rinha/thread_pool_200.h"

#define MAX_EVENTS 10
#define BUFFER_SIZE 4096

ABSL_FLAG(std::string, socket_path, "/tmp/unix_socket_example.sock",
          "path to socket file");

// Function to set a file descriptor (socket) to non-blocking mode
int make_socket_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    std::cerr << "fcntl F_GETFL" << std::endl;
    return -1;
  }

  flags |= O_NONBLOCK;
  int s = fcntl(fd, F_SETFL, flags);
  if (s == -1) {
    std::cerr << "fcntl F_SETFL" << std::endl;
    return -1;
  }

  return 0;
}

// Function to create a Unix socket and bind it to a specified path
int create_and_bind(const char *path) {
  struct sockaddr_un addr;
  int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sfd == -1) {
    std::cerr << "socket" << std::endl;
    return -1;
  }

  memset(&addr, 0, sizeof(struct sockaddr_un));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
    std::cerr << "bind" << std::endl;
    return -1;
  }

  return sfd;
}

int main(int argc, char *argv[]) {
  absl::ParseCommandLine(argc, argv);

  int server_fd, success;
  int epoll_fd;

  struct epoll_event event;
  struct epoll_event *events;

  std::string socket_path = absl::GetFlag(FLAGS_socket_path);
  LOG(INFO) << "Socket path: " << socket_path;
  server_fd = create_and_bind(socket_path.c_str());
  if (server_fd == -1)
    abort();

  success = make_socket_non_blocking(server_fd);
  if (success == -1)
    abort();

  success = listen(server_fd, SOMAXCONN);
  if (success == -1) {
    std::cerr << "listen" << std::endl;
    abort();
  }

  epoll_fd = epoll_create1(0);
  if (epoll_fd == -1) {
    std::cerr << "epoll_create" << std::endl;
    abort();
  }

  event.data.fd = server_fd;
  event.events = EPOLLIN | EPOLLET; // Read operation | Edge-triggered
  success = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);
  if (success == -1) {
    std::cerr << "epoll_ctl" << std::endl;
    abort();
  }

  // Buffer where events are returned
  events = (epoll_event *)calloc(MAX_EVENTS, sizeof(event));

  rinha::InitializeThreadPool(10);

  // The event loop
  while (true) {
    int n, i;

    n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    for (i = 0; i < n; i++) {
      if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
          (!(events[i].events & EPOLLIN))) {
        // An error has occured on this fd, or the socket is not
        // ready for reading (why were we notified then?)
        std::cerr << "epoll error" << std::endl;
        close(events[i].data.fd);
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
            } else {
              std::cerr << "accept" << std::endl;
              break;
            }
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
            std::cerr << "epoll_ctl" << std::endl;
            abort();
          }
        }
        continue;
      } else {
        // We have data on the fd waiting to be read. Read and
        // display it. We must read whatever data is available
        // completely, as we are running in edge-triggered mode
        // and won't get a notification again for the same
        // data.
        int done = 0;

        while (true) {
          ssize_t count;
          char buf[BUFFER_SIZE];

          count = read(events[i].data.fd, buf, sizeof buf);
          if (count == -1) {
            // If errno == EAGAIN, that means we have read all
            // data. So go back to the main loop.
            if (errno != EAGAIN) {
              std::cerr << "read" << std::endl;
              done = 1;
            }
            break;
          } else if (count == 0) {
            // End of file. The remote has closed the
            // connection.
            done = 1;
            break;
          }

          rinha::EnqueueProcessRequest(events[i].data.fd);
        }

        if (done) {
          std::cout << "Closed connection on descriptor " << events[i].data.fd
                    << std::endl;

          // Closing the descriptor will make epoll remove it
          // from the set of descriptors which are monitored.
          close(events[i].data.fd);
        }
      }
    }
  }

  free(events);
  close(server_fd);

  return EXIT_SUCCESS;
}
