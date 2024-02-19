#include "rinha/request_handler.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "glog/logging.h"

#include "rinha/from_http.h"
#include "rinha/in_memory_database.h"
#include "rinha/to_json.h"

namespace rinha {

Result HandleRequest(const char (&buffer)[1024], std::string *response_body) {
  Request request;
  bool success = FromHttp(buffer, &request);

  if (!success) {
    DLOG(ERROR) << "Failed to parse request" << std::endl;
    return Result::INVALID_REQUEST;
  }

  if (request.type == RequestType::BALANCE) {
    Customer *customer = GetCustomer(request.id);
    if (customer == nullptr) {
      DLOG(ERROR) << "Customer not found" << std::endl;
      return Result::NOT_FOUND;
    }
    std::string timestamp = absl::FormatTime(absl::Now());
    *response_body = CustomerToJson(*customer, std::move(timestamp));
    return Result::SUCCESS;
  }

  request.transaction.timestamp = absl::FormatTime(absl::Now());
  TransactionResult result =
      ExecuteTransaction(request.id, std::move(request.transaction));
  if (result == TransactionResult::NOT_FOUND) {
    DLOG(ERROR) << "Customer not found" << std::endl;
    return Result::NOT_FOUND;
  }

  if (result == TransactionResult::LIMIT_EXCEEDED) {
    DLOG(ERROR) << "Limit exceeded" << std::endl;
    return Result::INVALID_REQUEST;
  }

  *response_body = TransactionResultToJson(*GetCustomer(request.id));
  return Result::SUCCESS;
}

} // namespace rinha
