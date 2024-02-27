#include "rinha/thread_pool.h"

#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "glog/logging.h"

#include "rinha/maria_database.h"
#include "rinha/request_processor.h"
#include "rinha/structs.h"

namespace rinha {
namespace {

std::vector<std::thread> workers;
std::queue<process_params> tasks;
std::mutex queue_mutex;
std::condition_variable condition;
bool stop;
} // namespace

void InitializeThreadPool(size_t num_threads) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back([] {
      CHECK(rinha::MariaInitializeThread());
      while (true) {
        static thread_local process_params task;
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          condition.wait(lock, [] { return stop || !tasks.empty(); });
          if (stop && tasks.empty())
            return;
          task = std::move(tasks.front());
          tasks.pop();
        }
        ProcessRequest(task.buffer_p, task.num_read, task.client_fd);
      }
    });
  }
}

void EnqueueProcessRequest(process_params &&f) {
  DLOG(INFO) << "Enqueueing process request";
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    tasks.emplace(std::move(f));
  }
  condition.notify_one();
}

} // namespace rinha
