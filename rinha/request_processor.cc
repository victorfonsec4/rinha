#include "rinha/request_processor.h"

#include <fcntl.h>
#include <iostream>
#include <ostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <vector>

#include "absl/strings/str_cat.h"
#include "glog/logging.h"

#include "rinha/from_http.h"
#include "rinha/request_handler.h"
#include "rinha/structs.h"

namespace rinha {
namespace {
constexpr char kOkHeader[] = "HTTP/1.1 200 OK\r\nContent-Type: "
                             "application/json\r\nContent-Length: ";
constexpr size_t kOkHeaderLength = sizeof(kOkHeader);

constexpr char kBadRequestHeader[] =
    "HTTP/1.1 422 Unprocessable Entity\r\nContent-Length: 0\r\n\r\n";
constexpr size_t kBadRequestHeaderLength = sizeof(kBadRequestHeader);

constexpr char kNotFoundHeader[] =
    "HTTP/1.1 404 NOT FOUND\r\nContent-Length: 0\r\n\r\n";
constexpr size_t kNotFoundHeaderLength = sizeof(kNotFoundHeader);

thread_local char transaction_result_buffer[140] =
    "HTTP/1.1 200 OK\r\nContent-Type: "              // 31
    "application/json\r\nContent-Length: 62\r\n\r\n" // 40
    "{\"limite\":                    , \"saldo\":                     }";
constexpr int kLimiteBeginIdx = 31 + 40 + 11 - 1;
constexpr int kLimiteEndIdx = 31 + 40 + 30 - 1;
constexpr int kSaldoBeginIdx = 31 + 40 + 41 - 1;
constexpr int kSaldoEndIdx = 31 + 40 + 61 - 1;

void WriteNumberToString(int number, char *str, int end_idx) {
  if (number == 0) {
    str[end_idx] = '0';
    return;
  }

  bool is_negative = (number < 0);
  if (is_negative) {
    number = -number;
  }
  int i = end_idx;
  while (number > 0) {
    str[i] = number % 10 + '0';
    number /= 10;
    i--;
  }
  if (is_negative) {
    str[i] = '-';
  }
}
void BuildTransactionResponse(char *prebuilt_buffer,
                              const CustomerAccount &acc) {
  // Clears the buffer before writing to it
  memset(prebuilt_buffer + kLimiteBeginIdx, ' ',
         kLimiteEndIdx - kLimiteBeginIdx + 1);
  memset(prebuilt_buffer + kSaldoBeginIdx, ' ',
         kSaldoEndIdx - kSaldoBeginIdx + 1);

  WriteNumberToString(acc.limit, prebuilt_buffer, kLimiteEndIdx);

  WriteNumberToString(acc.balance, prebuilt_buffer, kSaldoEndIdx);
}

} // namespace

void ProcessRequest(ProcessRequestParams &&params) {
  ssize_t num_read = params.num_read;
  int client_fd = params.client_fd;
  std::vector<char> &buffer = *params.buffer_p;
  buffer[num_read] = '\0';
  DLOG(INFO) << "Received " << num_read << " bytes";
  DLOG(INFO) << "Received request: " << std::endl << buffer.data();

  // TODO: can we use the same buffer for the response?
  static thread_local std::string response_body(kOkHeaderLength + 1024, '\0');
  rinha::Request request;
  bool success = rinha::FromHttp(buffer.data(), &request);

  rinha::Result result;
  CustomerAccount acc;
  if (!success) {
    result = rinha::Result::INVALID_REQUEST;
  } else {
    result = rinha::HandleRequest(std::move(request), &response_body, &acc);
    DLOG(INFO) << "Response json body: " << response_body;
  }

  const char *http_response = kOkHeader;
  size_t http_response_length = kOkHeaderLength;

  std::string payload_response;
  switch (result) {
  case rinha::Result::SUCCESS: {
    if (request.type == rinha::RequestType::TRANSACTION) {
      BuildTransactionResponse(transaction_result_buffer, acc);
      http_response = transaction_result_buffer;
      http_response_length = sizeof(transaction_result_buffer);
      break;
    } else {
      // TODO: can we avoid this copy?
      payload_response = absl::StrCat(kOkHeader, response_body.size(),
                                      "\r\n\r\n", response_body);
      http_response = payload_response.c_str();
      http_response_length = payload_response.size();
      break;
    }
  }
  case rinha::Result::INVALID_REQUEST:
    http_response = kBadRequestHeader;
    // Don't include null terminator
    http_response_length = kBadRequestHeaderLength - 1;
    break;
  case rinha::Result::NOT_FOUND:
    http_response = kNotFoundHeader;
    // Don't include null terminator
    http_response_length = kNotFoundHeaderLength - 1;
    break;
  default:
    DCHECK(false);
    break;
  }

  // Send response
  DLOG(INFO) << "Sending response: " << std::endl
             << http_response << std::endl
             << std::endl
             << "to socket: " << client_fd;
  DLOG(INFO) << "Sending response to socket: " << client_fd;
  ssize_t write_count = 0;
  bool close_connection = false;
  while (write_count < http_response_length) {
    ssize_t count = write(client_fd, http_response + write_count,
                          http_response_length - write_count);
    DLOG(INFO) << "Wrote " << count << " bytes";
    if (count == -1) {
      // If errno == EAGAIN, that means we have written all
      // data. So go back to the main loop.
      if (errno != EAGAIN) {
        LOG(ERROR) << "Error reading from client: " << strerror(errno);
        close_connection = true;
      }
      break;
    } else if (count == 0) {
      // End of file. The remote has closed the
      // connection.
      DLOG(ERROR) << "Wrote 0 bytes to client";
      close_connection = true;
      break;
    }
    write_count += count;
  }

  DLOG(INFO) << "Sent " << write_count << " bytes out of "
             << http_response_length;
  if (close_connection) {
    // Close the connection
    DLOG(ERROR) << "Closing connection";
    close(client_fd);
  }
  static int total_write_count = 0;
  total_write_count++;
  DLOG(INFO) << "Write count: " << total_write_count;
}

} // namespace rinha
