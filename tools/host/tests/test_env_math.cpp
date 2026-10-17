// core/env_math and core/heater_policy (SPEC §6.2, Appendix B).
#include <initializer_list>

#include "core/env_math.h"
#include "core/heater_policy.h"
#include "tiny_test.h"

using namespace hg;

TEST(env_conversions) {
  CHECK_NEAR(sht4xTempC(0x6666), 25.00f, 0.005f);
  CHECK_NEAR(sht4xRh(0x8000), 56.50f, 0.005f);
  CHECK_NEAR(sht3xRh(0x8000), 50.00f, 0.005f);
  CHECK_NEAR(sht3xTempC(0x6666), 25.00f, 0.005f);
  CHECK_NEAR(sht4xRh(0), 0.0f, 0.0001f);                   // -6 % clamped
  CHECK_NEAR(sht4xRh(0xFFFF), 100.0f, 0.0001f);            // 119 % clamped
  CHECK_NEAR(cToF(-40.0f), -40.0f, 0.0001f);
  CHECK_NEAR(cToF(25.0f), 77.0f, 0.0001f);
}

TEST(env_rh_correction_dew_point_abs_humidity) {
  CHECK_NEAR(correctRhForTemp(50.0f, 25.0f, 23.0f), 56.38f, 0.01f);
  CHECK_NEAR(dewPointC(25.0f, 50.0f), 13.85f, 0.01f);
  CHECK_NEAR(dewPointC(23.4f, 56.0f), 14.12f, 0.01f);
  CHECK_NEAR(absHumidity(23.4f, 56.0f), 11.75f, 0.01f);
  CHECK_NEAR(absHumidity(25.0f, 50.0f), 11.48f, 0.01f);
  // the correction keeps the dew point
  CHECK_NEAR(dewPointC(23.0f, correctRhForTemp(50.0f, 25.0f, 23.0f)), dewPointC(25.0f, 50.0f), 0.01f);
  CHECK_NEAR(correctRhForTemp(95.0f, 20.0f, 15.0f), 100.0f, 0.0001f);   // clamped
}

namespace {
HeaterIn due(HeaterMode m) {
  HeaterIn in{};
  in.mode = m; in.supported = true; in.now = MonoUs{3600LL * kUsPerS};
  in.tValid = true; in.tC = 20.0f;
  return in;
}
}  // namespace

TEST(heater_modes) {
  CHECK_EQ(heaterDue(due(HeaterMode::Off)), HeaterDecision::No);
  CHECK_EQ(heaterDue(due(HeaterMode::Every5)), HeaterDecision::Pulse);      // never pulsed yet
  HeaterIn in = due(HeaterMode::Every5);
  in.havePulse = true; in.lastPulse = MonoUs{in.now.v - 299 * kUsPerS};
  CHECK_EQ(heaterDue(in), HeaterDecision::No);
  in.lastPulse = MonoUs{in.now.v - 300 * kUsPerS};
  CHECK_EQ(heaterDue(in), HeaterDecision::Pulse);
  in.mode = HeaterMode::Every60;
  CHECK_EQ(heaterDue(in), HeaterDecision::No);
  HeaterIn a = due(HeaterMode::Auto);
  CHECK_EQ(heaterDue(a), HeaterDecision::No);                               // RH not high
  a.highRhStreak = 2;
  CHECK_EQ(heaterDue(a), HeaterDecision::Pulse);
  a.havePulse = true; a.lastPulse = MonoUs{a.now.v - 599 * kUsPerS};      // at most every 10 min
  CHECK_EQ(heaterDue(a), HeaterDecision::No);
}

TEST(heater_guards) {
  HeaterIn in = due(HeaterMode::Every5);
  in.supported = false;   CHECK_EQ(heaterDue(in), HeaterDecision::SkipUnsupported);
  in = due(HeaterMode::Every5); in.busy = true;     CHECK_EQ(heaterDue(in), HeaterDecision::No);
  in = due(HeaterMode::Every5); in.tC = 60.5f;      CHECK_EQ(heaterDue(in), HeaterDecision::SkipHot);
  in.tValid = false;                                 CHECK_EQ(heaterDue(in), HeaterDecision::Pulse);
  in = due(HeaterMode::Every5); in.shutdownPending = true;
  CHECK_EQ(heaterDue(in), HeaterDecision::SkipShutdown);                     // even when a pulse is due
  // 60 s spacing (§6.2): whatever the mode and RH, never a pulse within 60 s of the last one
  for (HeaterMode m : {HeaterMode::Auto, HeaterMode::Every5, HeaterMode::Every10, HeaterMode::Every30, HeaterMode::Every60}) {
    HeaterIn s = due(m);
    s.highRhStreak = 9; s.havePulse = true; s.lastPulse = MonoUs{s.now.v - 59 * kUsPerS};
    CHECK(heaterDue(s) != HeaterDecision::Pulse);
  }
  CHECK_EQ(heaterCommand(HeaterPower::Low20mW), 0x1E);
  CHECK_EQ(heaterCommand(HeaterPower::Mid110mW), 0x2F);
  CHECK_EQ(heaterCommand(HeaterPower::High200mW), 0x39);
}
