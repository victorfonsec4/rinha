#ifndef RINHA_REQUEST_HANDLER_H
#define RINHA_REQUEST_HANDLER_H

#include <vector>

#include "rinha/structs.h"

namespace rinha {

Result HandleRequest(const std::vector<char> buffer,
                     std::string *response_body);

} // namespace rinha

#endif
