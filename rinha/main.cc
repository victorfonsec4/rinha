#include <fcntl.h>
#include <iostream>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/status/status.h"
#include "absl/synchronization/barrier.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "glog/logging.h"

#include "rinha/from_http.h"
#include "rinha/io_thread_pool.h"
#include "rinha/maria_database.h"
#include "rinha/request_handler.h"
#include "rinha/structs.h"
#include "rinha/thread_pool.h"

ABSL_FLAG(std::string, socket_path, "/tmp/unix_socket_example.sock",
          "path to socket file");

ABSL_FLAG(int, num_process_threads, 20,
          "Number of threads for requesting processing");

ABSL_FLAG(int, num_connection_threads, 1,
          "Number of threads for handling connections");

namespace {

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

  LOG(INFO) << "Initializing threads...";
  rinha::InitializeThreadPool(num_process_threads);
  rinha::InitializeIoThreadPool(num_connection_threads, epoll_fd, server_fd);

  LOG(INFO) << "Socket path: " << socket_path;
  LOG(INFO) << "Number of process threads: " << num_process_threads;
  LOG(INFO) << "Number of connection threads: " << num_connection_threads;

  DLOG(INFO) << "Server ready for connections.";
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(50));
  }

  return 0;
}
