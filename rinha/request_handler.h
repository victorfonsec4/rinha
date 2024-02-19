#ifndef RINHA_REQUEST_HANDLER_H
#define RINHA_REQUEST_HANDLER_H

#include "rinha/structs.h"

namespace rinha {

Result HandleRequest(const char (&buffer)[1024], std::string * response_body);

} //namespace rinha

#endif
