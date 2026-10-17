// core/rtc_bridge: seal/verify, decideRestore (§5.4 table), deep-sleep arithmetic, phantom wakes (§7.5).
#include <cstring>

#include "core/rtc_bridge.h"
#include "tiny_test.h"

using namespace hg;

namespace {
MonoUs at(int64_t ms) { return MonoUs{ms * 1000}; }
RtcState sealed() { RtcState s; memset(&s, 0, sizeof s); s.targetDeg = -1; rtcSeal(s); return s; }
const ResetReason kNonCrash[] = {ResetReason::Sw, ResetReason::Usb, ResetReason::Jtag};
const ResetReason kCrash[] = {ResetReason::Panic, ResetReason::IntWdt, ResetReason::TaskWdt, ResetReason::Wdt,
                              ResetReason::CpuLockup};
const ResetReason kPowerOn[] = {ResetReason::PowerOn, ResetReason::Brownout, ResetReason::Ext, ResetReason::Sdio,
                                ResetReason::Efuse, ResetReason::PwrGlitch, ResetReason::Unknown};
}  // namespace

TEST(rtc_seal_verify_and_corruption) {
  RtcState s = sealed();
  CHECK(rtcVerify(s)); CHECK(!rtcMagicOnly(s));
  CHECK_EQ(s.magic, 0x48473252u);
  CHECK(sizeof(RtcState) <= 600);                           // ~520 B of RTC memory
  s.page = 3;
  CHECK(!rtcVerify(s)); CHECK(rtcMagicOnly(s));             // CRC damage
  rtcSeal(s);
  CHECK(rtcVerify(s));
  s.layout = kRtcLayout + 1;                                // a layout bump after an update
  CHECK(!rtcVerify(s)); CHECK(rtcMagicOnly(s));
  RtcState g; memset(&g, 0xA5, sizeof g);
  CHECK(!rtcVerify(g)); CHECK(!rtcMagicOnly(g));
}

TEST(rtc_decide_restore_every_row) {
  RtcState ok = sealed();
  RtcState bad = ok; bad.page = 9;                          // magic only
  RtcState garbage; memset(&garbage, 0x5A, sizeof garbage);
  CHECK_EQ(decideRestore(ResetReason::DeepSleep, ok), RestoreVerdict::Restore);
  CHECK_EQ(decideRestore(ResetReason::DeepSleep, bad), RestoreVerdict::Lost);
  CHECK_EQ(decideRestore(ResetReason::DeepSleep, garbage), RestoreVerdict::Lost);
  for (ResetReason r : kNonCrash) {
    CHECK_EQ(decideRestore(r, ok), RestoreVerdict::Restore);
    CHECK_EQ(decideRestore(r, bad), RestoreVerdict::Lost);
    CHECK_EQ(decideRestore(r, garbage), RestoreVerdict::Fresh);
  }
  RtcState loop = ok; loop.crashCount = 3; rtcSeal(loop);
  RtcState two = ok; two.crashCount = 2; rtcSeal(two);
  for (ResetReason r : kCrash) {
    CHECK_EQ(decideRestore(r, two), RestoreVerdict::RestoreCrash);
    CHECK_EQ(decideRestore(r, loop), RestoreVerdict::CrashLoop);
    CHECK_EQ(decideRestore(r, bad), RestoreVerdict::Lost);
    CHECK_EQ(decideRestore(r, garbage), RestoreVerdict::Fresh);
  }
  for (ResetReason r : kPowerOn) CHECK_EQ(decideRestore(r, ok), RestoreVerdict::Fresh);
}

TEST(rtc_ten_minute_deep_sleep_models) {
  Stopwatch sw; Countdown tm;
  sw.start(at(0));
  tm.setPreset(3);                                          // 10 min
  tm.start(at(0));
  RtcState s = sealed();
  s.savedAt = RtcUs{5000000000ULL};
  s.sw = sw.save(at(30000));
  s.tm = tm.save(at(30000));                                // 9:30 left
  rtcSeal(s);
  const RtcUs wake{s.savedAt.v + 600ULL * 1000000};         // 10 min later
  const int64_t slept = sleptUs(s, wake);
  CHECK_EQ(slept, (int64_t)600 * kUsPerS);
  CHECK_EQ(sleptUs(s, RtcUs{s.savedAt.v - 5}), (int64_t)0); // clamped
  Stopwatch sw2; Countdown tm2;
  sw2.restore(s.sw, slept, at(300));
  tm2.restore(s.tm, slept, at(300));
  CHECK_EQ(sw2.elapsedUs(at(300)), (int64_t)630 * kUsPerS);
  CHECK(tm2.tick(at(300)));                                 // expired during sleep: alert once
  CHECK(!tm2.tick(at(400)));
  CHECK_EQ(tm2.overtimeUs(at(300)), (int64_t)30 * kUsPerS); // correct overtime
}

TEST(rtc_phantom_wake_rule_and_window) {
  // tap released before the sample after a 10 min sleep -> not phantom
  CHECK(!isPhantomWake(WakeCause::Ext1, 0x02, 0x00, 600LL * kUsPerS));
  // no key LOW after a 1 s sleep -> phantom
  CHECK(isPhantomWake(WakeCause::Ext1, 0x02, 0x00, 1LL * kUsPerS));
  CHECK(!isPhantomWake(WakeCause::Ext1, 0x02, 0x02, 1LL * kUsPerS));   // key still down: real
  CHECK(!isPhantomWake(WakeCause::Timer, 0x00, 0x00, 1LL * kUsPerS));
  // 5 in 2 min -> exclusion
  RtcState s = sealed();
  bool ex = false;
  for (int i = 0; i < 5; ++i) ex = countPhantomWake(s, RtcUs{(uint64_t)(1000 + i * 20) * 1000000});
  CHECK(ex);
  // 4 in 2 min + 1 after 3 min -> no exclusion (the window restarted)
  RtcState t = sealed();
  for (int i = 0; i < 4; ++i) CHECK(!countPhantomWake(t, RtcUs{(uint64_t)(1000 + i * 20) * 1000000}));
  CHECK(!countPhantomWake(t, RtcUs{(uint64_t)(1000 + 180) * 1000000}));
  CHECK_EQ(t.phantomWakes, 1);
}
