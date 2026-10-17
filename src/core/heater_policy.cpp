#include "heater_policy.h"

#include "../config/timing.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }

int64_t periodMs(HeaterMode m) {
  switch (m) {
    case HeaterMode::Every5:  return 5 * 60000;
    case HeaterMode::Every10: return 10 * 60000;
    case HeaterMode::Every30: return 30 * 60000;
    case HeaterMode::Every60: return 60 * 60000;
    default:                  return 0;
  }
}
}  // namespace

HeaterDecision heaterDue(const HeaterIn& in) {
  if (in.mode == HeaterMode::Off) return HeaterDecision::No;
  const int64_t since = in.havePulse ? in.now - in.lastPulse : INT64_MAX;
  bool due;
  if (in.mode == HeaterMode::Auto) due = in.highRhStreak >= 2 && since >= ms(kHeaterAutoMinGapMs);
  else due = since >= periodMs(in.mode) * kUsPerMs;
  if (!due) return HeaterDecision::No;
  if (!in.supported) return HeaterDecision::SkipUnsupported;
  if (in.shutdownPending) return HeaterDecision::SkipShutdown;
  if (in.busy) return HeaterDecision::No;                        // a measurement is in flight: try again later
  if (since < ms(kHeaterMinSpacingMs)) return HeaterDecision::SkipSpacing;
  if (in.tValid && in.tC > kHeaterMaxAmbientC) return HeaterDecision::SkipHot;
  return HeaterDecision::Pulse;
}

uint8_t heaterCommand(HeaterPower p) {
  switch (p) {
    case HeaterPower::Low20mW:   return 0x1E;
    case HeaterPower::High200mW: return 0x39;
    default:                     return 0x2F;
  }
}

}  // namespace hg
