#ifndef RINHA_MARIA_DATABASE_H
#define RINHA_MARIA_DATABASE_H

#include "rinha/structs.h"

namespace rinha {
bool MariaInitializeThread();
bool MariaInitializeDb();
bool MariaDbGetCustomer(int id, Customer *customer);
TransactionResult MariaDbExecuteTransaction(int id, Transaction &&transaction,
                                            CustomerAccount *acc);
} // namespace rinha

#endif
