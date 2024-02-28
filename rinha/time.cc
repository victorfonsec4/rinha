#include "rinha/time.h"

#include <string>

#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace rinha {
namespace {
thread_local char time_str[20] = "a";
thread_local absl::Time last_update = absl::Now();
constexpr absl::Duration kCalculateInterval = absl::Seconds(30);
} // namespace

char *GetTime() {
  if ((absl::Now() - last_update > kCalculateInterval) || time_str[0] == 'a') {
    last_update = absl::Now();
    // 2024-02-27 21:36
    strncpy(
        time_str,
        absl::FormatTime("%E4Y-%m-%d %H:%M", last_update, absl::LocalTimeZone())
            .c_str(),
        20);
  }
  return time_str;
}
} // namespace rinha
