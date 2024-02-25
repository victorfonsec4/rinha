#include "rinha/shared_count.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "glog/logging.h"

namespace rinha {
namespace {
constexpr char kSharedMemoryName[][15] = {"shared_count_1", "shared_count_2",
                                          "shared_count_3", "shared_count_4",
                                          "shared_count_5"};

constexpr int kSharedMemorySize = sizeof(int);
int *shared_counters[5];
} // namespace

bool InitializeSharedCounters() {
  LOG(INFO) << "Initializing shared counters";
  int shm_fd;
  for (int i = 0; i < 5; i++) {
    // Create shared memory object
    shm_fd = shm_open(kSharedMemoryName[i], O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
      perror("shm_open");
      return false;
    }

    // Size the shared memory
    if (ftruncate(shm_fd, kSharedMemorySize) == -1) {
      perror("ftruncate");
      return false;
    }

    // Map the shared memory in the process address space
    shared_counters[i] = static_cast<int *>(mmap(
        0, kSharedMemorySize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0));
    if (shared_counters[i] == MAP_FAILED) {
      perror("mmap");
      return false;
    }

    *shared_counters[i] = 0;
  }

  return true;
}

int GetSharedCount(int i) { return *shared_counters[i]; }
void IncreaseSharedCount(int i) { (*shared_counters[i])++; }
} // namespace rinha
