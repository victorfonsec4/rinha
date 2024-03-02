#ifndef RINHA_TO_JSON_H
#define RINHA_TO_JSON_H

#include <string>

#include "rinha/structs.h"

namespace rinha {
std::string CustomerToJson(const Customer &customer,
                           std::string &&data_extrato);
} // namespace rinha

#endif
