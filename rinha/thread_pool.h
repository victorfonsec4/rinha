#ifndef RINHA_THREAD_POOL_H
#define RINHA_THREAD_POOL_H

#include "rinha/structs.h"

namespace rinha {
void InitializeThreadPool(size_t num_threads,
                          absl::string_view handler_socket_hostname);
void EnqueueProcessRequest(ProcessRequestParams &&f);
} // namespace rinha

#endif
