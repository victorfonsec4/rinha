#ifndef RINHA_IO_THREAD_POOL_H
#define RINHA_IO_THREAD_POOL_H

namespace rinha {
void InitializeIoThreadPool(size_t num_threads, int epoll_fd, int server_fd);
} //namespace rinha

#endif
