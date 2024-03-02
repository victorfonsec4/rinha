#include "rinha/thread_pool_router.h"

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <sys/epoll.h>
#include <thread>
#include <vector>

#include "glog/logging.h"

namespace rinha {
namespace {

std::vector<std::thread> workers;
std::queue<Message> tasks;
std::mutex queue_mutex;
std::condition_variable condition;
bool stop;
} // namespace

void InitializeThreadPool(size_t num_threads) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back([] {
      while (true) {
        static thread_local Message m;
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          condition.wait(lock, [] { return stop || !tasks.empty(); });
          if (stop && tasks.empty())
            return;
          m = tasks.front();
          tasks.pop();
        }

        write(m.fd, m.buffer, m.size);
      }
    });
  }
}

void EnqueueProcessRequest(Message &&m) {
  DLOG(INFO) << "Enqueueing process request";
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    tasks.emplace(m);
  }

  condition.notify_one();
}

} // namespace rinha
