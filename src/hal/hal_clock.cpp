#include "build_checks.h"
#include "hal_ports.h"

#include <esp_rtc_time.h>
#include <esp_timer.h>
#include <sys/time.h>

namespace hg {

MonoUs HalMonoClock::now() const { return MonoUs{esp_timer_get_time()}; }   // compensated across light sleep

RtcUs HalRtcClock::now() const { return RtcUs{esp_rtc_get_time_us()}; }      // survives deep sleep and SW resets

bool HalWallClock::get(UtcMs* out) const {
  struct timeval tv;
  if (gettimeofday(&tv, nullptr) != 0) return false;
  out->v = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
  return true;
}

// The only place in the firmware that sets the system time (lint rule, SPEC §4.2).
bool HalWallClock::set(UtcMs t) {
  struct timeval tv;
  tv.tv_sec = (time_t)(t.v / 1000);
  tv.tv_usec = (suseconds_t)((t.v % 1000) * 1000);
  return settimeofday(&tv, nullptr) == 0;
}

}  // namespace hg
