#include "rinha/in_memory_database.h"

namespace rinha {
namespace {
// TODO: if we start threading then we need to protect these with a mutex.
// Probably split into separate variables so we can lock only the one we need.
Customer customers[5] = {
    {.limit = 100000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},
    {.limit = 80000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},
    {.limit = 1000000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},
    {.limit = 10000000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},
    {.limit = 500000,
     .balance = 0,
     .transactions = {},
     .transaction_count = 0,
     .next_transaction_index = 0},

};
} // namespace

Customer *GetCustomer(int id) {
  if (id > 5 || id < 1) {
    return nullptr;
  }
  return &customers[id - 1];
}

TransactionResult ExecuteTransaction(int id, Transaction &&transaction) {
  if (id > 5 || id < 1) {
    return TransactionResult::NOT_FOUND;
  }
  Customer *customer = &customers[id - 1];

  if (customer->balance + transaction.value < customer->limit) {
    return TransactionResult::LIMIT_EXCEEDED;
  }

  customer->balance += transaction.value;
  customer->transactions[customer->next_transaction_index] =
      std::move(transaction);
  customer->next_transaction_index =
      (customer->next_transaction_index + 1) % 10;
  customer->transaction_count++;

  return TransactionResult::SUCCESS;
}
} // namespace rinha
