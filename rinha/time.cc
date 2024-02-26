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
} // namespace

std::string GetCurrentTimeString() {
  if (absl::Now() - last_update > kCalculateInterval) {
    time_str =
        absl::FormatTime("%Y-%m-%d %H:%M", absl::Now(), absl::LocalTimeZone());
    last_update = absl::Now();
  }
  return time_str;
}
int64_t GetTime() { return absl::ToUnixSeconds(absl::Now()); }
std::string FormatTime(int64_t timestamp) {
  return absl::FormatTime("%Y-%m-%d %H:%M", absl::FromUnixSeconds(timestamp),
                          absl::LocalTimeZone());
}
} // namespace rinha
