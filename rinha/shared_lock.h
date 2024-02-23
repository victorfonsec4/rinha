#ifndef RINHA_SHARED_LOCK_H
#define RINHA_SHARED_LOCK_H

namespace rinha {
bool InitializeSharedLocks();

void GetSharedLock(int i);

void ReleaseSharedLock(int i);
} //namespace rinha

#endif
