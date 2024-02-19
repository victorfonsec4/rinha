#ifndef RINHA_IN_MEMORY_DATABASE_H
#define RINHA_IN_MEMORY_DATABASE_H

#include "rinha/structs.h"

namespace rinha {
Customer *get_customer(int id);
TransactionResult ExecuteTransaction(int id, Transaction&& transaction);
} // namespace rinha

#endif
