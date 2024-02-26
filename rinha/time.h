#ifndef RINHA_TIME_H
#define RINHA_TIME_H

#include <string>

namespace rinha {
int64_t GetTime();
std::string FormatTime(int64_t timestamp);
std::string GetCurrentTimeString();
} // namespace rinha

#endif
