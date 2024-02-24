#include "rinha/shared_lock.h"

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
constexpr char kSharedMemoryName[][16] = {"shared_memory_1", "shared_memory_2",
                                          "shared_memory_3", "shared_memory_4",
                                          "shared_memory_5"};
constexpr int kSharedMemorySize = sizeof(pthread_mutex_t);

pthread_mutex_t *shared_locks[5];
} // namespace

bool InitializeSharedLocks() {
  LOG(INFO) << "Initializing shared locks";
  int shm_fd;
  pthread_mutexattr_t mutexAttr;
  for (int i = 0; i < 5; i++) {
    // Create shared memory object
    DLOG(INFO) << "Creating shared memory " << kSharedMemoryName[i];
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
    DLOG(INFO) << "Mapping shared memory " << kSharedMemoryName[i];
    shared_locks[i] = (pthread_mutex_t *)mmap(
        0, kSharedMemorySize, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_locks[i] == MAP_FAILED) {
      perror("mmap");
      return false;
    }

    // Initialize mutex attribute
    DLOG(INFO) << "Initializing mutex attribute";
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);

    DLOG(INFO) << "Initializing mutex";
    // Initialize the mutex
    pthread_mutex_init(shared_locks[i], &mutexAttr);
  }

  DLOG(INFO) << "Shared locks initialized";

  return true;
}

void GetSharedLock(int i) { pthread_mutex_lock(shared_locks[i]); }

void ReleaseSharedLock(int i) { pthread_mutex_unlock(shared_locks[i]); }

} // namespace rinha
