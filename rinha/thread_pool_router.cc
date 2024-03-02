#include "rinha/thread_pool_router.h"

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <oneapi/tbb/concurrent_queue.h>
#include <queue>
#include <sys/epoll.h>
#include <thread>
#include <vector>

#include "glog/logging.h"

namespace rinha {
namespace {
constexpr unsigned int kQueueSize = 1024;
tbb::concurrent_bounded_queue<unsigned int> queue;

constexpr unsigned int kMessageBufferSize = 1024;
Message messages[kMessageBufferSize];
int current_message_idx = 0;

std::vector<std::thread> workers;
thread_local std::queue<Message> tasks;
std::condition_variable condition;

constexpr unsigned int kBufferSize = 4096;
thread_local char buf[kBufferSize];
} // namespace

void InitializeThreadPool(size_t num_threads) {
  queue.set_capacity(kQueueSize);
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back([] {
      while (true) {
        unsigned int idx;
        queue.pop(idx);
        bool closed_connection = false;
        ssize_t total_count = 0;

        const Message &m = messages[idx];
        int src = m.src_fd;
        int dst = m.dst_fd;

        while (true) {
          ssize_t count;

          DLOG(INFO) << "Reading from client: " << src;
          count = read(src, buf + total_count, kBufferSize - total_count);
          DLOG(INFO) << "Read " << count << " bytes";
          if (count == -1) {
            // If errno == EAGAIN, that means we have read all
            // data. So go back to the main loop.
            if (errno != EAGAIN) {
              LOG(ERROR) << "read error : " << strerror(errno);
              DCHECK(false);
              closed_connection = true;
            }
            break;
          } else if (count == 0) {
            // End of file. The remote has closed the
            // connection.
            closed_connection = true;
            DCHECK(false);
            break;
          }
          if (count > 0) {
            total_count += count;
          }
        }

        if (total_count > 0 && !closed_connection) {
          // Get the corresponding file descriptor
          DLOG(INFO) << "Fd association is " << src << " -> " << dst;

          DLOG(INFO) << "Received message from fd " << src << " with size "
                     << total_count << " bytes";

          write(dst, buf, total_count);
        }
        if (closed_connection) {
          DCHECK(false);
        }
      }
    });
  }
}

void EnqueueProcessRequest(Message &&m) {
  messages[current_message_idx] = std::move(m);
  queue.push(current_message_idx);
  current_message_idx = (current_message_idx + 1) % kMessageBufferSize;
}

} // namespace rinha
