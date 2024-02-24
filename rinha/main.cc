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
#include "simdjson.h"

#include "rinha/maria_database.h"
#include "rinha/moustique.h"
#include "rinha/request_handler.h"
#include "rinha/shared_lock.h"

ABSL_FLAG(std::string, socket_path, "/tmp/unix_socket_example.sock",
          "path to socket file");

ABSL_FLAG(int, num_process_threads, 20,
          "Number of threads for requesting processing");

ABSL_FLAG(int, num_connection_threads, 2,
          "Number of threads for handling connections");

constexpr char kOkHeader[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ";
constexpr size_t kOkHeaderLength = sizeof(kOkHeader);

constexpr char kBadRequestHeader[] =
    "HTTP/1.1 422 Unprocessable Entity\r\n\r\n";
constexpr size_t kBadRequestHeaderLength = sizeof(kBadRequestHeader);

constexpr char kNotFoundHeaderLength[] = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
constexpr size_t NotFoundHeaderLength = sizeof(kNotFoundHeaderLength);

constexpr int kBufferWrittableSize = 1024;
constexpr int kBufferTotalSize =
    kBufferWrittableSize + simdjson::SIMDJSON_PADDING;

namespace {
void ProcessRequest(std::vector<char> &&buffer, ssize_t num_read,
                    std::function<ssize_t(char *, int)> read,
                    std::function<ssize_t(const char *, int)> write) {
  // TODO: We might be able to skip this.
  if (num_read >= kBufferWrittableSize - 1) {
    LOG(WARNING) << "Message too big";
    // discard the rest of the message in the socket
    while (num_read >= buffer.size() - 1) {
      num_read = read(buffer.data(), buffer.size());
    }
    ssize_t result = write(kBadRequestHeader, kBadRequestHeaderLength);
    if (result == -1) {
      DLOG(ERROR) << "Failed to send response";
    }
    return;
  }

  buffer[num_read] = '\0';
  DLOG(INFO) << "Received " << num_read << " bytes";
  DLOG(INFO) << "Received request: " << std::endl << buffer.data();

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
  DLOG(INFO) << "Sending response: " << std::endl << http_response;
  ssize_t r = write(http_response, http_response_length);
  if (r == -1) {
    DLOG(ERROR) << "Failed to send response";
  }
}

void SetNonBlocking(int socket_fd) {
  int flags = fcntl(socket_fd, F_GETFL, 0);
  if (flags == -1) {
    std::cerr << "Failed to get socket flags";
    exit(EXIT_FAILURE);
  }
  if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    std::cerr << "Failed to set non-blocking socket";
    exit(EXIT_FAILURE);
  }
}
} // namespace

int main(int argc, char *argv[]) {
  absl::ParseCommandLine(argc, argv);

  LOG(INFO) << "Starting server";
  DLOG(INFO) << "Size of customer: " << sizeof(rinha::Customer);

  int num_process_threads = absl::GetFlag(FLAGS_num_process_threads);
  ThreadPool process_pool(num_process_threads);

  int server_fd;
  struct sockaddr_un server_addr;
  // Create socket and bind server socket
  {
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
      LOG(ERROR) << "Failed to create socket";
      return -1;
    }

    SetNonBlocking(server_fd);

    std::string socket_path = absl::GetFlag(FLAGS_socket_path);
    LOG(INFO) << "Socket path: " << socket_path;

    // Bind socket to socket path
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path.c_str(),
            sizeof(server_addr.sun_path) - 1);
    unlink(socket_path.c_str()); // Remove the socket if it already exists
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
        -1) {
      LOG(ERROR) << "Bind failed";
      LOG(ERROR) << "Error: " << strerror(errno);
      close(server_fd);
      return -1;
    }
  }

  // Initialize database and locks
  CHECK(rinha::MariaInitializeDb());
  CHECK(rinha::InitializeSharedLocks());

  int num_connection_threads = absl::GetFlag(FLAGS_num_connection_threads);

  LOG(INFO) << "Number of process threads: " << num_process_threads;
  LOG(INFO) << "Number of connection threads: " << num_connection_threads;

  auto handle_lambda = [](int client_fd, auto read, auto write) {
    std::vector<char> buffer(kBufferTotalSize);
    ssize_t num_read = read(buffer.data(), kBufferWrittableSize - 1);
    if (num_read == -1) {
      DLOG(ERROR) << "Failed to read from socket";
      close(client_fd);
      return;
    }

    ProcessRequest(std::move(buffer), num_read, read, write);
  };

  moustique_listen_fd(server_fd, num_connection_threads, handle_lambda);

  return 0;
}
