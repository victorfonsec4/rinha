#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "ThreadPool.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/synchronization/barrier.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "glog/logging.h"
#include "simdjson.h"

#include "rinha/maria_database.h"
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

constexpr char kNotFoundHeader[] = "HTTP/1.1 404 NOT FOUND\r\n\r\n";
constexpr size_t kNotFoundHeaderLength = sizeof(kNotFoundHeader);

constexpr int kBufferWrittableSize = 1024;
constexpr int kBufferTotalSize =
    kBufferWrittableSize + simdjson::SIMDJSON_PADDING;
constexpr int kMaxEvents = 256;

namespace {
void ProcessRequest(std::vector<char> &&buffer, ssize_t num_read,
                    int client_fd) {
  buffer[num_read] = '\0';
  DLOG(INFO) << "Received " << num_read << " bytes";
  DLOG(INFO) << "Received request: " << std::endl << buffer.data();

  // TODO: can we use the same buffer for the response?
  std::string response_body(kOkHeaderLength + 1024, '\0');
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

void signalHandler(int signum) { exit(signum); }
} // namespace

int main(int argc, char *argv[]) {
  absl::ParseCommandLine(argc, argv);

  signal(SIGINT, signalHandler);

  LOG(INFO) << "Starting server";
  DLOG(INFO) << "Size of customer: " << sizeof(rinha::Customer);

  int num_process_threads = absl::GetFlag(FLAGS_num_process_threads);
  int num_connection_threads = absl::GetFlag(FLAGS_num_connection_threads);

  ThreadPool process_pool(num_process_threads);
  ThreadPool connection_pool(num_connection_threads);

  int server_fd;
  struct sockaddr_un server_addr;
  int epoll_fd = epoll_create1(0);
  std::string socket_path = absl::GetFlag(FLAGS_socket_path);
  // Create socket and bind server socket
  {
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd == -1) {
      LOG(ERROR) << "Failed to create socket";
      return -1;
    }

    SetNonBlocking(server_fd);

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

    if (listen(server_fd, 5) == -1) {
      LOG(ERROR) << "Failled to listen on the socket: " << strerror(errno);
      close(server_fd);
      return 1;
    }

    epoll_event accept_event;
    accept_event.events = EPOLLIN;
    accept_event.data.fd = server_fd;

    if (epoll_fd == -1) {
      LOG(ERROR) << "Failed to create epoll fd: " << strerror(errno);
      return 1;
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &accept_event) == -1) {
      LOG(ERROR) << "Failed to add server socket to epoll" << strerror(errno);
      close(server_fd);
      return 1;
    }
  }

  // Initialize database and locks
  CHECK(rinha::MariaInitializeDb());
  CHECK(rinha::InitializeSharedLocks());

  LOG(INFO) << "Initializing database threads...";
  absl::Barrier barrier(num_process_threads + 1);
  for (int i = 0; i < num_process_threads; i++) {
    process_pool.enqueue([&barrier]() {
      CHECK(rinha::MariaInitializeThread());
      barrier.Block();
    });
  }
  barrier.Block();
  LOG(INFO) << "Database threads initialized.";

  LOG(INFO) << "Socket path: " << socket_path;
  LOG(INFO) << "Number of process threads: " << num_process_threads;
  LOG(INFO) << "Number of connection threads: " << num_connection_threads;

  connection_pool.enqueue([epoll_fd, server_fd, &process_pool]() {
    std::vector<epoll_event> events(kMaxEvents);
    while (true) {
      int num_events = epoll_wait(epoll_fd, events.data(), kMaxEvents, -1);
      for (int i = 0; i < num_events; i++) {
        // Look for new connections
        if (events[i].data.fd == server_fd) {
          int client_fd = accept(server_fd, NULL, NULL);
          if (client_fd == -1) {
            LOG(ERROR) << "Failed to accept new connection: "
                       << strerror(errno);
            continue;
          }

          epoll_event client_event;
          client_event.events = EPOLLIN | EPOLLET;
          client_event.data.fd = client_fd;

          SetNonBlocking(events[i].data.fd);

          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) ==
              -1) {
            LOG(ERROR) << "Failed to add client socket to epoll: "
                       << strerror(errno);
            close(client_fd);
            continue;
          }
          // Handle connection data
        } else {
          std::vector<char> buffer(kBufferTotalSize);
          // Save a spot for the null terminator
          ssize_t count =
              read(events[i].data.fd, buffer.data(), kBufferWrittableSize - 1);
          if (count == -1) {
            LOG(ERROR) << "Failed to read from socket: " << strerror(errno);
            close(events[i].data.fd);
            continue;
          } else if (count == 0) {
            // Client disconnected
            close(events[i].data.fd);
            continue;
          }

          process_pool.enqueue([b = std::move(buffer), count,
                                client_fd = events[i].data.fd]() mutable {
            ProcessRequest(std::move(b), count, client_fd);
          });
        }
      }
    }
  });

  return 0;
}
