#ifndef RINHA_THREAD_POOL_H
#define RINHA_THREAD_POOL_H

#include "rinha/structs.h"

namespace rinha {
void InitializeThreadPool(size_t num_threads);
void EnqueueProcessRequest(ProcessRequestParams &&f);
} // namespace rinha

#endif
