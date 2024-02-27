#include "rinha/request_processor.h"

#include <iostream>
#include <ostream>
#include <vector>

#include "absl/strings/str_cat.h"
#include "glog/logging.h"

#include "rinha/from_http.h"
#include "rinha/request_handler.h"

namespace rinha {
namespace {
constexpr char kOkHeader[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
constexpr size_t kOkHeaderLength = sizeof(kOkHeader);

constexpr char kBadRequestHeader[] =
    "HTTP/1.1 422 Unprocessable Entity\r\n\r\n";
constexpr size_t kBadRequestHeaderLength = sizeof(kBadRequestHeader);

constexpr char kNotFoundHeader[] = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
constexpr size_t kNotFoundHeaderLength = sizeof(kNotFoundHeader);
} // namespace

void ProcessRequest(std::vector<char> *buffer_p, ssize_t num_read,
                    int client_fd) {
  std::vector<char> &buffer = *buffer_p;
  buffer[num_read] = '\0';
  DLOG(INFO) << "Received " << num_read << " bytes";
  DLOG(INFO) << "Received request: " << std::endl << buffer.data();

  // TODO: can we use the same buffer for the response?
  static thread_local std::string response_body(kOkHeaderLength + 1024, '\0');
  rinha::Request request;
  bool success = rinha::FromHttp(buffer.data(), &request);

  rinha::Result result;
  if (!success) {
    result = rinha::Result::INVALID_REQUEST;
  } else {
    result = rinha::HandleRequest(std::move(request), &response_body);
  }

  const char *http_response = kOkHeader;
  size_t http_response_length = kOkHeaderLength;

  std::string payload_response;
  switch (result) {
  case rinha::Result::SUCCESS: {
    // TODO: can we avoid this copy?
    payload_response = absl::StrCat(kOkHeader, response_body.size(), "\r\n\r\n",
                                    response_body);
    http_response = payload_response.c_str();
    http_response_length = payload_response.size();
    break;
  }
  case rinha::Result::INVALID_REQUEST:
    http_response = kBadRequestHeader;
    http_response_length = kBadRequestHeaderLength;
    break;
  case rinha::Result::NOT_FOUND:
    http_response = kNotFoundHeader;
    http_response_length = kNotFoundHeaderLength;
    break;
  default:
    DCHECK(false);
    break;
  }

  // Send response
  DLOG(INFO) << "Sending response: " << std::endl << http_response;
  ssize_t r = write(client_fd, http_response, http_response_length);
  if (r == -1) {
    LOG(ERROR) << "Failed to send response: " << strerror(errno);
  }

  // TODO: Is there some kinda of keep alive that can be implemented here?
  close(client_fd);
}

} // namespace rinha
