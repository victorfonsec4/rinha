#ifndef RINHA_FROM_HTTP_H
#define RINHA_FROM_HTTP_H

#include <string>

#include "rinha/structs.h"

#include "absl/strings/string_view.h"

namespace rinha {
Request from_http(absl::string_view http);
} // namespace rinha

#endif
