#ifndef RINHA_SQLITE_DATABASE_H
#define RINHA_SQLITE_DATABASE_H

#include "rinha/structs.h"

namespace rinha {
bool InitializeDb();
bool DbGetCustomer(int id, Customer *customer);
TransactionResult DbExecuteTransaction(int id, Transaction &&transaction);
} // namespace rinha

#endif
