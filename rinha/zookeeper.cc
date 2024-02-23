#include "zoo_lock.h"
#include <memory>
#define THREADED

#include "rinha/zookeeper.h"

#include <cstdlib>

#include <zookeeper.h>

#include "absl/synchronization/notification.h"
#include "glog/logging.h"
#include "zookeeper.h"

namespace rinha {
namespace {
char kLockPath[][8] = {"/a", "/b", "/c", "/d", "/e"};
zkr_lock_mutex zoo_mutex[5];

zhandle_t *z_handle;



std::unique_ptr<absl::Notification> lock_acquired[5];

void lock_completion(int rc, void *data) {
  int i = *static_cast<int *>(data);
  DLOG(INFO) << "Watcher called with rc " << rc << " for id " << i;
  lock_acquired[i]->Notify();
}

} // namespace

bool InitializeZooKeeper() {
  const char *host = "127.0.0.1:2181"; // ZooKeeper server IP:Port
  int timeout = 30000;                 // Timeout for session in milliseconds

  // Create a ZooKeeper handle
  z_handle = zookeeper_init(host, nullptr, timeout, 0, 0, 0);
  if (z_handle == nullptr) {
    LOG(ERROR) << "Error connecting to ZooKeeper server!";
    return false;
  }

  return true;
}

void GetZooLock(int i) {
  DLOG(INFO) << "Attempting to acquire lock " << i;

  lock_acquired[i] = std::make_unique<absl::Notification>();

  int rc = zkr_lock_init(&zoo_mutex[i], z_handle, kLockPath[i],
                            &ZOO_OPEN_ACL_UNSAFE);
  DLOG(INFO) << "Lock init returned " << rc;

  if (rc != ZOK) {
    LOG(ERROR) << "Error initializing lock " << i;
    return;
  }

  DLOG(INFO) << "Getting lock " << i;
  rc = zkr_lock_lock(&zoo_mutex[i]);
  DLOG(INFO) << "Lock lock returned " << rc;
  if (rc != ZOK) {
    LOG(ERROR) << "Error locking: " << rc;
    return;
  }
}

void ReleaseZooLock(int i) {
  DLOG(INFO) << "Releasing lock " << i;
  zkr_lock_unlock(&zoo_mutex[i]);
}

} // namespace rinha
