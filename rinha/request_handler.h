#ifndef RINHA_REQUEST_HANDLER_H
#define RINHA_REQUEST_HANDLER_H

#include <vector>

#include "rinha/structs.h"

namespace rinha {

Result HandleRequest(Request &&request, std::string *response_body,
                     Customer *customer);

} // namespace rinha

#endif
