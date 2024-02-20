#include "rinha/request_handler.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "glog/logging.h"

#include "rinha/from_http.h"
// #include "rinha/in_memory_database.h"
#include "rinha/to_json.h"
#include "rinha/sqlite_database.h"

namespace rinha {

Result HandleRequest(const std::vector<char> buffer,
                     std::string *response_body) {
  Request request;
  bool success = FromHttp(buffer.data(), &request);

  if (!success) {
    DLOG(ERROR) << "Failed to parse request" << std::endl;
    return Result::INVALID_REQUEST;
  }

  if (request.type == RequestType::BALANCE) {
    // Customer *customer;
    // customer = GetCustomer(request.id);
    // if (customer == nullptr) {
    //   DLOG(ERROR) << "Customer not found" << std::endl;
    //   return Result::NOT_FOUND;
    // }

    Customer customer;
    bool success = DbGetCustomer(request.id, &customer);
    if (!success) {
      DLOG(ERROR) << "Customer not found" << std::endl;
      return Result::NOT_FOUND;
    }

    std::string timestamp = absl::FormatTime(absl::Now());
    *response_body = CustomerToJson(customer, std::move(timestamp));
    return Result::SUCCESS;
  }

  // TODO: can we avoid this copy?
  std::string time_str = absl::FormatTime(absl::Now());
  if (time_str.size() >= sizeof(request.transaction.timestamp)) {
    LOG(ERROR) << "TIMESTAMP TOO LONG" << std::endl;
    return Result::INVALID_REQUEST;
  }
  std::strncpy(request.transaction.timestamp, time_str.data(),
               sizeof(request.transaction.timestamp) - 1);
  request.transaction.timestamp[sizeof(request.transaction.timestamp) -1 ] = '\0';

  // TransactionResult result =
  //     ExecuteTransaction(request.id, std::move(request.transaction));

  TransactionResult result =
      DbExecuteTransaction(request.id, std::move(request.transaction));
  if (result == TransactionResult::NOT_FOUND) {
    DLOG(ERROR) << "Customer not found" << std::endl;
    return Result::NOT_FOUND;
  }

  if (result == TransactionResult::LIMIT_EXCEEDED) {
    DLOG(ERROR) << "Limit exceeded" << std::endl;
    return Result::INVALID_REQUEST;
  }

  Customer customer;
  success = DbGetCustomer(request.id, &customer);
  if (!success) {
    DLOG(ERROR) << "Customer not found" << std::endl;
    return Result::NOT_FOUND;
  }
  *response_body = TransactionResultToJson(customer);
  return Result::SUCCESS;
}

} // namespace rinha
