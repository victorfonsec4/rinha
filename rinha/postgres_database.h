#ifndef RINHA_POSTGRES_DATABASE_H
#define RINHA_POSTGRES_DATABASE_H

#include "rinha/structs.h"

namespace rinha {
  bool PostgresInitializeDb();
  bool PostgresDbGetCustomer(int id, Customer *customer);
TransactionResult PostgresDbExecuteTransaction(int id, Transaction &&transaction,
                                               Customer *customer);
} //namespace rinha

#endif
