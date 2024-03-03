#ifndef RINHA_THREAD_POOL_ROUTER_H
#define RINHA_THREAD_POOL_ROUTER_H

#include <sys/epoll.h>

namespace rinha {
struct Message {
  int src_fd;
  int dst_fd;
};

void InitializeThreadPool(size_t num_threads);
} // namespace rinha

#endif
