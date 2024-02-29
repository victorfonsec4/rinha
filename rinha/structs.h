#ifndef RINHA_STRUCTS_H
#define RINHA_STRUCTS_H

#include <string>
#include <vector>

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

struct ProcessRequestParams {
  std::vector<char> *buffer_p;
  ssize_t num_read;
  int client_fd;
  int epoll_fd;
};
} // namespace rinha

#endif
