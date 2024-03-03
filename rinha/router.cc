#include "glog/logging.h"

#include "thread_pool_router.h"
#include <thread>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"

ABSL_FLAG(int, num_threads, 10, "Number of threads to use.");

int main(int argc, char *argv[]) {
  absl::ParseCommandLine(argc, argv);
  int num_threads = absl::GetFlag(FLAGS_num_threads);

  DLOG(INFO) << "Starting revese proxy with " << num_threads << " threads.";

  rinha::InitializeThreadPool(num_threads);
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(50));
  }

  return EXIT_SUCCESS;
}
