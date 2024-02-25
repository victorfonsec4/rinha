#ifndef RINHA_SHARED_COUNT_H
#define RINHA_SHARED_COUNT_H

namespace rinha {
bool InitializeSharedCounters();
int GetSharedCount(int i);
void IncreaseSharedCount(int i);
} //namespace rinha

#endif
