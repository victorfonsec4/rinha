#include "rinha/thread_pool.h"

#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <vector>

#include "glog/logging.h"

namespace rinha {
namespace {
std::vector<std::thread> workers;

constexpr int kBufferWrittableSize = 1024;
constexpr int kBufferTotalSize = kBufferWrittableSize + 1;
constexpr int kMaxEvents = 256;
constexpr int kMaxConnections = 3000;

void SetNonBlocking(int socket_fd) {
  int flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags == -1) {
    std::cerr << "Failed to get socket flags";
    exit(EXIT_FAILURE);
  }
  if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    std::cerr << "Failed to set non-blocking socket";
    exit(EXIT_FAILURE);
  }
}
} // namespace

void InitializeIoThreadPool(size_t num_threads, int epoll_fd, int server_fd) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back([epoll_fd, server_fd] {
      std::vector<epoll_event> events(kMaxEvents);
      DLOG(INFO) << "Starting io thread";
      while (true) {
        int num_events = epoll_wait(epoll_fd, events.data(), kMaxEvents, -1);
        for (int i = 0; i < num_events; i++) {
          // Look for new connections
          if (events[i].data.fd == server_fd) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd == -1) {
              DLOG(ERROR) << "Failed to accept new connection: "
                          << strerror(errno);
              continue;
            }

            epoll_event client_event;
            client_event.events = EPOLLIN | EPOLLET;
            client_event.data.fd = client_fd;

            SetNonBlocking(events[i].data.fd);

            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) ==
                -1) {
              LOG(ERROR) << "Failed to add client socket to epoll: "
                         << strerror(errno);
              close(client_fd);
              continue;
            }
            // Handle connection data
          } else {
            static thread_local std::vector<std::vector<char>> buffers(
                kMaxConnections, std::vector<char>(kBufferTotalSize));
            static thread_local unsigned int buffer_index = 0;
            ssize_t count =
                read(events[i].data.fd, buffers[buffer_index].data(),
                     kBufferWrittableSize - 1);
            if (count == -1) {
              LOG(ERROR) << "Failed to read from socket: " << strerror(errno);
              close(events[i].data.fd);
              continue;
            } else if (count == 0) {
              // Client disconnected
              close(events[i].data.fd);
              continue;
            }

            rinha::process_params params;
            params.buffer_p = &buffers[buffer_index];
            params.num_read = count;
            params.client_fd = events[i].data.fd;
            EnqueueProcessRequest(std::move(params));

            buffer_index = (buffer_index + 1) % kMaxConnections;
          }
        }
      }
    });
  }
}

} // namespace rinha
