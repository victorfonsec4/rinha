#ifndef RINHA_ZOOKEEPER_H
#define RINHA_ZOOKEEPER_H

namespace rinha {
bool InitializeZooKeeper();

void GetZooLock(int i);

void ReleaseZooLock(int i);
} // namespace rinha

#endif
