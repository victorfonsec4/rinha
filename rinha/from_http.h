#ifndef RINHA_FROM_HTTP_H
#define RINHA_FROM_HTTP_H

#include <string>

#include "rinha/structs.h"

#include "absl/strings/string_view.h"

namespace rinha {
bool FromHttp(absl::string_view http, Request *request);
} // namespace rinha

#endif
