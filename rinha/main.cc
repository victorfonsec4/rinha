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

constexpr char OkHeader[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";

constexpr char BadRequestHeader[] = "HTTP/1.1 422 Unprocessable Entity\r\n\r\n";
constexpr size_t BadRequestHeaderLength = sizeof(BadRequestHeader);

constexpr char NotFoundHeader[] = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
constexpr size_t NotFoundHeaderLength = sizeof(NotFoundHeader);

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
    read(client_fd, buffer, sizeof(buffer) - 1);
    DLOG(INFO) << "Received request: " << std::endl << buffer << std::endl;

    std::string response_body;
    rinha::Result result = rinha::HandleRequest(buffer, &response_body);

    const char *http_response;
    size_t http_response_length;

    switch (result) {
    case rinha::Result::SUCCESS: {
      // TODO: can we avoid this copy?
      std::string response = absl::StrCat(OkHeader, response_body.size(),
                                          "\r\n", response_body, "\r\n\r\n");
      http_response = response.c_str();
      http_response_length = response.size();
      break;
    }
    case rinha::Result::INVALID_REQUEST:
      http_response = BadRequestHeader;
      http_response_length = BadRequestHeaderLength;
      break;
    case rinha::Result::NOT_FOUND:
      http_response = NotFoundHeader;
      http_response_length = NotFoundHeaderLength;
      break;
    default:
      break;
    }

    write(client_fd, http_response, http_response_length);

    // Close connection
    close(client_fd);
  }

  // Cleanup
  close(server_fd);
  unlink(socket_path);

  return 0;
}
