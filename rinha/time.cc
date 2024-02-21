#include "rinha/time.h"

#include <string>

#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace rinha {
namespace {
thread_local std::string time_str =
    absl::FormatTime("%Y-%m-%d %H:%M", absl::Now(), absl::LocalTimeZone());
thread_local absl::Time last_update = absl::Now();
constexpr absl::Duration kCalculateInterval = absl::Seconds(30);
} //namespace


std::string GetTime() {
  if (absl::Now() - last_update > kCalculateInterval) {
    last_update = absl::Now();
    time_str = absl::FormatTime("%Y-%m-%d %H:%M", last_update, absl::LocalTimeZone());
  }
  return time_str;
}
}


