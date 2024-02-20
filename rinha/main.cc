#include <fcntl.h>
#include <iostream>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ThreadPool.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "glog/logging.h"

#include "rinha/request_handler.h"
#include "rinha/sqlite_database.h"

ABSL_FLAG(std::string, socket_path, "/tmp/unix_socket_example.sock",
          "path to socket file");

constexpr char kOkHeader[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
constexpr size_t kOkHeaderLength = sizeof(kOkHeader);

constexpr char kBadRequestHeader[] =
    "HTTP/1.1 422 Unprocessable Entity\r\n\r\n";
constexpr size_t kBadRequestHeaderLength = sizeof(kBadRequestHeader);

constexpr char kNotFoundHeaderLength[] = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
constexpr size_t NotFoundHeaderLength = sizeof(kNotFoundHeaderLength);
constexpr int kProcessThreadCount = 50;
constexpr int kConnectionThreadCount = 5;

namespace {
void ProcessRequest(std::vector<char> &&buffer, int num_read, int client_fd) {
  if (num_read >= buffer.size() - 1) {
    LOG(WARNING) << "Message too big" << std::endl;
    // discard the rest of the message in the socket
    while (num_read >= buffer.size() - 1) {
      num_read = read(client_fd, buffer.data(), buffer.size());
    }
    ssize_t result =
        write(client_fd, kBadRequestHeader, kBadRequestHeaderLength);
    if (result == -1) {
      DLOG(ERROR) << "Failed to send response" << std::endl;
    }
    close(client_fd);
    return;
  }

  buffer[num_read] = '\0';
  DLOG(INFO) << "Received " << num_read << " bytes" << std::endl;
  DLOG(INFO) << "Received request: " << std::endl << buffer.data() << std::endl;

  std::string response_body;
  rinha::Result result = rinha::HandleRequest(buffer, &response_body);

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
    http_response = kNotFoundHeaderLength;
    http_response_length = NotFoundHeaderLength;
    break;
  default:
    DCHECK(false);
    break;
  }

  // Send response
  DLOG(INFO) << "Sending response: " << std::endl << http_response << std::endl;
  ssize_t r = write(client_fd, http_response, http_response_length);
  if (r == -1) {
    DLOG(ERROR) << "Failed to send response" << std::endl;
  }

  // Close connection
  close(client_fd);
}

void SetNonBlocking(int socket_fd) {
  int flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags == -1) {
    std::cerr << "Failed to get socket flags" << std::endl;
    exit(EXIT_FAILURE);
  }
  if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    std::cerr << "Failed to set non-blocking socket" << std::endl;
    exit(EXIT_FAILURE);
  }
}
} // namespace

int main(int argc, char *argv[]) {
  absl::ParseCommandLine(argc, argv);
  CHECK(rinha::InitializeDb());
  ThreadPool process_pool(kProcessThreadCount);
  ThreadPool connection_pool(kConnectionThreadCount);

  int server_fd, client_fd;
  struct sockaddr_un server_addr, client_addr;
  socklen_t client_addr_size = sizeof(client_addr);

  // Create socket
  server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd == -1) {
    LOG(ERROR) << "Failed to create socket" << std::endl;
    return -1;
  }

  SetNonBlocking(server_fd);

  std::string socket_path = absl::GetFlag(FLAGS_socket_path);
  LOG(INFO) << "Socket path: " << socket_path << std::endl;

  // Bind socket to socket path
  server_addr.sun_family = AF_UNIX;
  strncpy(server_addr.sun_path, socket_path.c_str(),
          sizeof(server_addr.sun_path) - 1);
  unlink(socket_path.c_str()); // Remove the socket if it already exists
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    LOG(ERROR) << "Bind failed" << std::endl;
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

  const int MAX_EVENTS = 50;
  struct epoll_event ev, events[MAX_EVENTS];
  int epollfd = epoll_create1(0);
  if (epollfd == -1) {
    LOG(ERROR) << "Failed to create epoll file descriptor" << std::endl;
    return -1;
  }

  ev.events = EPOLLIN;
  ev.data.fd = server_fd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
    LOG(ERROR) << "Failed to add file descriptor to epoll" << std::endl;
    close(server_fd);
    return -1;
  }

  // Accept connections
  while (true) {
    int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
    if (nfds == -1) {
      LOG(ERROR) << "Epoll wait failed" << std::endl;
      break;
    }

    for (int n = 0; n < nfds; ++n) {
      if (events[n].data.fd == server_fd) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr,
                           &client_addr_size);
        if (client_fd == -1) {
          LOG(ERROR) << "Accept failed: " << strerror(errno) << std::endl;
          continue;
        }

        // SetNonBlocking(client_fd);

        process_pool.enqueue([client_fd]() {
          std::vector<char> buffer(1024);
          ssize_t num_read = read(client_fd, buffer.data(), buffer.size() - 1);
          if (num_read == -1) {
            DLOG(ERROR) << "Failed to read from socket" << std::endl;
            close(client_fd);
            return;
          }

          ProcessRequest(std::move(buffer), num_read, client_fd);
        });
      }
    }
  }

  // Cleanup
  close(server_fd);
  unlink(socket_path.c_str());

  return 0;
}
