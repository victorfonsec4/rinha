#ifndef RINHA_STRUCTS_H
#define RINHA_STRUCTS_H

#include <string>

namespace rinha {

enum class TransactionResult { SUCCESS, LIMIT_EXCEEDED, NOT_FOUND };
enum class RequestType { TRANSACTION, BALANCE };
enum class Result { SUCCESS, INVALID_REQUEST, NOT_FOUND };

struct Transaction {
  int value;
  char description[11];
  char timestamp[20];
};

struct Customer {
  int limit;
  int balance;
  Transaction transactions[10];
  char transaction_count;
  char next_transaction_index;
};

struct Request {
  RequestType type;
  int id;
  Transaction transaction;
};
} // namespace rinha

#endif
