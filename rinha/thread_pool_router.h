#ifndef RINHA_THREAD_POOL_ROUTER_H
#define RINHA_THREAD_POOL_ROUTER_H

#include <sys/epoll.h>

namespace rinha {
struct Message {
  int fd;
  char *buffer;
  size_t size;
};

void InitializeThreadPool(size_t num_threads);
void EnqueueProcessRequest(Message &&m);
} // namespace rinha

#endif
