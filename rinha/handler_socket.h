#ifndef RINHA_HANDLER_SOCKET_H
#define RINHA_HANDLER_SOCKET_H

#include "rinha/structs.h"

namespace rinha {
bool InitializeHs(absl::string_view host);
bool ReadCustomerHs(int id, Customer *customer);
bool WriteCustomerHs(int id, const Customer &customer);
} // namespace rinha

#endif
