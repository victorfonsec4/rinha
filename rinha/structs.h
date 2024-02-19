#ifndef RINHA_STRUCTS_H
#define RINHA_STRUCTS_H

#include <string>

namespace rinha {

enum class TransactionResult { SUCCESS, LIMIT_EXCEEDED, NOT_FOUND };
enum class RequestType { TRANSACTION, BALANCE, INVALID };

struct Transaction {
  int value; char description[11]; std::string timestamp;
};

struct Customer {
  int limit;
  int balance;
  Transaction transactions[10];
  int transaction_count;
  int next_transaction_index;
};

struct Request {
  RequestType type;
  int id;
  Transaction transaction;
};
} //namespace rinha

#endif
