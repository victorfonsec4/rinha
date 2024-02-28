#ifndef RINHA_THREAD_POOL_200_H
#define RINHA_THREAD_POOL_200_H

#include <sys/epoll.h>

namespace rinha {
void InitializeThreadPool(size_t num_threads);
void EnqueueProcessRequest(int client_fd);
} //namespace rinha

#endif
