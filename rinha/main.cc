#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "absl/status/status.h"
#include "glog/logging.h"

const char *socket_path = "/tmp/unix_socket_example.sock";

int main() {
  int server_fd, client_fd;
  struct sockaddr_un server_addr, client_addr;
  socklen_t client_addr_size;
  char buffer[1024] = {0};
  const char *http_response =
      "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
      "0\r\n\r\n";

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

    // Send HTTP response
    write(client_fd, http_response, strlen(http_response));

    // Close connection
    close(client_fd);
  }

  // Cleanup
  close(server_fd);
  unlink(socket_path);

  return 0;
}
