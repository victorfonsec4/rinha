#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "absl/status/status.h"
#include "glog/logging.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"

#include "rinha/request_handler.h"

const char *socket_path = "/tmp/unix_socket_example.sock";

constexpr char kOkHeader[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
constexpr size_t kOkHeaderLength = sizeof(kOkHeader);

constexpr char kBadRequestHeader[] =
    "HTTP/1.1 422 Unprocessable Entity\r\n\r\n";
constexpr size_t kBadRequestHeaderLength = sizeof(kBadRequestHeader);

constexpr char kNotFoundHeaderLength[] = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
constexpr size_t NotFoundHeaderLength = sizeof(kNotFoundHeaderLength);

int main() {
  int server_fd, client_fd;
  struct sockaddr_un server_addr, client_addr;
  socklen_t client_addr_size;
  char buffer[1024] = {0};

  // Create socket
  server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd == -1) {
    DLOG(ERROR) << "Failed to create socket" << std::endl;
    return -1;
  }

  // Bind socket to socket path
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);
  unlink(socket_path); // Remove the socket if it already exists
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    DLOG(ERROR) << "Bind failed" << std::endl;
    close(server_fd);
    return -1;
  }

  // Listen for connections
  if (listen(server_fd, 5) == -1) {
    DLOG(ERROR) << "Listen failed" << std::endl;
    close(server_fd);
    return -1;
  }

  DLOG(INFO) << "Server listening on " << socket_path << std::endl;

  // Accept connections
  while (true) {
    client_addr_size = sizeof(client_addr);
    client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_size);
    if (client_fd == -1) {
      DLOG(ERROR) << "Accept failed" << std::endl;
      continue; // Continue accepting other connections
    }

    // Read request (this example does not parse the request)
    ssize_t num_read = read(client_fd, buffer, sizeof(buffer) - 1);
    if (num_read == -1) {
      DLOG(ERROR) << "Failed to read request" << std::endl;
      close(client_fd);
      continue; // Continue accepting other connections
    }

    if (num_read >= sizeof(buffer) - 1) {
      DLOG(INFO) << "Message too big" << std::endl;
      // discard the rest of the message in the socket
      while (num_read == sizeof(buffer) - 1) {
        num_read = read(client_fd, buffer, sizeof(buffer) - 1);
      }
      ssize_t result =
          write(client_fd, kBadRequestHeader, kBadRequestHeaderLength);
      if (result == -1) {
        DLOG(ERROR) << "Failed to send response" << std::endl;
      }
      close(client_fd);
      continue; // Continue accepting other connections
    }

    buffer[num_read] = '\0';
    DLOG(INFO) << "Received request: " << std::endl << buffer << std::endl;

    std::string response_body;
    rinha::Result result = rinha::HandleRequest(buffer, &response_body);

    const char *http_response = kOkHeader;
    size_t http_response_length = kOkHeaderLength;

    std::string payload_response;
    switch (result) {
    case rinha::Result::SUCCESS: {
      // TODO: can we avoid this copy?
      payload_response = absl::StrCat(kOkHeader, response_body.size(),
                                      "\r\n\r\n", response_body);
      http_response = payload_response.c_str();
      http_response_length = payload_response.size();
      break;
    }
    case rinha::Result::INVALID_REQUEST:
      http_response = kBadRequestHeader;
      http_response_length = kBadRequestHeaderLength;
      break;
    case rinha::Result::NOT_FOUND:
      http_response = kNotFoundHeaderLength;
      http_response_length = NotFoundHeaderLength;
      break;
    default:
      DCHECK(false);
      break;
    }

    // Send response
    DLOG(INFO) << "Sending response: " << std::endl
               << http_response << std::endl;
    ssize_t r = write(client_fd, http_response, http_response_length);
    if (r == -1) {
      DLOG(ERROR) << "Failed to send response" << std::endl;
    }

    // Close connection
    close(client_fd);
  }

  // Cleanup
  close(server_fd);
  unlink(socket_path);

  return 0;
}
