#ifndef RINHA_SQLITE_DATABASE_H
#define RINHA_SQLITE_DATABASE_H

#include "rinha/structs.h"

namespace rinha {
bool SqliteInitializeDb();
bool SqliteDbGetCustomer(int id, Customer *customer);
TransactionResult SqliteDbExecuteTransaction(int id, Transaction &&transaction);
} // namespace rinha

#endif
