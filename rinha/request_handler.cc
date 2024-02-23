#include "rinha/request_handler.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "glog/logging.h"

#include "rinha/from_http.h"
#include "rinha/to_json.h"
#include "rinha/maria_database.h"
#include "rinha/time.h"

namespace rinha {

Result HandleRequest(const std::vector<char> buffer,
                     std::string *response_body) {
  Request request;
  bool success = FromHttp(buffer.data(), &request);

  if (!success) {
    DLOG(ERROR) << "Failed to parse request" << std::endl;
    return Result::INVALID_REQUEST;
  }

  std::string current_time = GetTime();

  if (request.type == RequestType::BALANCE) {
    Customer customer;
    bool success = MariaDbGetCustomer(request.id, &customer);
    if (!success) {
      DLOG(ERROR) << "Customer not found" << std::endl;
      return Result::NOT_FOUND;
    }

    *response_body = CustomerToJson(customer, std::move(current_time));
    return Result::SUCCESS;
  }

  // TODO: we can prob get rid of this check.
  if (current_time.size() >= sizeof(request.transaction.timestamp)) {
    LOG(ERROR) << "TIMESTAMP TOO LONG" << std::endl;
    return Result::INVALID_REQUEST;
  }
  std::strncpy(request.transaction.timestamp, current_time.data(),
               sizeof(request.transaction.timestamp) - 1);
  request.transaction.timestamp[sizeof(request.transaction.timestamp) -1 ] = '\0';

  Customer customer;
  TransactionResult result =
      MariaDbExecuteTransaction(request.id, std::move(request.transaction), &customer);
  if (result == TransactionResult::NOT_FOUND) {
    DLOG(ERROR) << "Customer not found" << std::endl;
    return Result::NOT_FOUND;
  }

  if (result == TransactionResult::LIMIT_EXCEEDED) {
    DLOG(ERROR) << "Limit exceeded" << std::endl;
    return Result::INVALID_REQUEST;
  }

  *response_body = TransactionResultToJson(customer);
  return Result::SUCCESS;
}

} // namespace rinha
