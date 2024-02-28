#ifndef RINHA_REQUEST_PROCESSOR_H
#define RINHA_REQUEST_PROCESSOR_H

#include <unistd.h>
#include <vector>

#include "rinha/structs.h"

namespace rinha {

void ProcessRequest(ProcessRequestParams &&params);

} // namespace rinha

#endif
