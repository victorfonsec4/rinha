#include "rinha/thread_pool_200.h"

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
std::queue<int> tasks;
std::mutex queue_mutex;
std::condition_variable condition;
bool stop;
} // namespace

void InitializeThreadPool(size_t num_threads) {
  for (size_t i = 0; i < num_threads; ++i) {
    workers.emplace_back([] {
      while (true) {
        static thread_local int client_fd;
        {
          std::unique_lock<std::mutex> lock(queue_mutex);
          condition.wait(lock, [] { return stop || !tasks.empty(); });
          if (stop && tasks.empty())
            return;
          client_fd = tasks.front();
          tasks.pop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        static const char *response = "HTTP/1.1 200 OK\r\n"
                                      "Content-Length: 0\r\n"
                                      "Connection: keep-alive\r\n"
                                      "\r\n";

        write(client_fd, response, strlen(response));
      }
    });
  }
}

void EnqueueProcessRequest(int client_fd) {
  DLOG(INFO) << "Enqueueing process request";
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    tasks.emplace(client_fd);
  }

  condition.notify_one();
}

} // namespace rinha
