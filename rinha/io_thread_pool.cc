#include "rinha/thread_pool.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "glog/logging.h"

namespace rinha {
namespace {
std::vector<std::thread> workers;

static constexpr int kBufferWrittableSize = 1024;
static constexpr int kBufferTotalSize = kBufferWrittableSize + 1;
static constexpr int kMaxEvents = 1000;
static constexpr int kMaxConnections = 3000;

void SetNonBlocking(int socket_fd) {
  int flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags == -1) {
    LOG(ERROR) << "Failed to get socket flags";
    exit(EXIT_FAILURE);
  }
  if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    LOG(ERROR) << "Failed to set non-blocking socket";
    exit(EXIT_FAILURE);
  }
}
} // namespace

void InitializeIoThreadPool(size_t num_threads, int epoll_fd, int server_fd) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back([epoll_fd, server_fd] {
      std::vector<epoll_event> events(kMaxEvents);
      static thread_local std::vector<std::vector<char>> buffers(
          kMaxConnections, std::vector<char>(kBufferTotalSize));

      static thread_local unsigned int buffer_index = 0;

      DLOG(INFO) << "Starting io thread";
      int read_count = 0;
      while (true) {
        int n, i;

        n = epoll_wait(epoll_fd, events.data(), kMaxEvents, -1);
        for (i = 0; i < n; i++) {
          const auto &event = events[i];
          DLOG(INFO) << "New epoll event: " << event.events;
          if ((event.events & EPOLLERR) || (event.events & EPOLLHUP) ||
              (!(event.events & EPOLLIN))) {
            // An error has occured on this fd, or the socket is not
            // ready for reading (why were we notified then?)
            DLOG(ERROR) << "epoll error: " << strerror(errno)
                        << " events: " << event.events;
            DLOG(ERROR) << "Closing connection on descriptor " << event.data.fd;
            close(event.data.fd);
            continue;
          } else if (server_fd == event.data.fd) {
            // We have a notification on the listening socket, which
            // means one or more incoming connections.
            DLOG(INFO) << "New connection on server_fd: " << server_fd;
            while (true) {
              DLOG(INFO) << "Accepting new connection";
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
                  DLOG(INFO) << "Accepting new connection";
                  break;
                }
              }

              DLOG(INFO) << "Accepted new connection: " << infd;

              // Make the incoming socket non-blocking and add it to the
              // list of fds to monitor.
              SetNonBlocking(infd);

              static struct epoll_event event;
              event.data.fd = infd;
              event.events = EPOLLIN | EPOLLET;
              int success = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, infd, &event);
              if (success == -1) {
                LOG(ERROR) << "epoll_ctl failed to add new connection";
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

            ssize_t total_count = 0;
            DLOG(INFO) << "Reading from client: " << event.data.fd;
            while (true) {
              ssize_t count;
              count = read(event.data.fd,
                           buffers[buffer_index].data() + total_count,
                           kBufferWrittableSize - total_count);
              DLOG(INFO) << "Read " << count << " bytes";
              if (count == -1) {
                // If errno == EAGAIN, that means we have read all
                // data. So go back to the main loop.
                if (errno != EAGAIN) {
                  LOG(ERROR)
                      << "Error reading from client: " << strerror(errno);
                  done = 1;
                }
                break;
              } else if (count == 0) {
                // End of file. The remote has closed the
                // connection.
                DLOG(INFO)
                    << "Received 0 bytes ,closed connection on descriptor "
                    << event.data.fd;
                done = 1;
                break;
              }
              if (count > 0) {
                total_count += count;
              }
            }

            if (total_count > 0) {
              read_count++;
              DLOG(INFO) << "Read count: " << read_count;

              ProcessRequestParams params;
              params.num_read = total_count;
              params.client_fd = event.data.fd;
              params.buffer_p = &buffers[buffer_index];
              rinha::EnqueueProcessRequest(std::move(params));

              buffer_index = (buffer_index + 1) % kMaxConnections;
            }

            if (done) {
              LOG(ERROR) << "Closed connection on descriptor " << event.data.fd;

              // Closing the descriptor will make epoll remove it
              // from the set of descriptors which are monitored.
              close(event.data.fd);
            }
          }
        }
      }
    });
  }
}

} // namespace rinha
