#include "rtc_bridge.h"

#include <stddef.h>

#include "../config/timing.h"
#include "crc.h"

namespace hg {
namespace {
uint32_t rtcCrc(const RtcState& s) { return crc32(&s, offsetof(RtcState, crc)); }

bool isCrash(ResetReason r) {
  return r == ResetReason::Panic || r == ResetReason::IntWdt || r == ResetReason::TaskWdt || r == ResetReason::Wdt ||
         r == ResetReason::CpuLockup;
}
}  // namespace

void rtcSeal(RtcState& s) {
  s.magic = kRtcMagic;
  s.layout = kRtcLayout;
  s.size = (uint16_t)sizeof(RtcState);
  s.crc = rtcCrc(s);
}

bool rtcVerify(const RtcState& s) {
  return s.magic == kRtcMagic && s.layout == kRtcLayout && s.size == sizeof(RtcState) && s.crc == rtcCrc(s);
}

bool rtcMagicOnly(const RtcState& s) { return s.magic == kRtcMagic && !rtcVerify(s); }

RestoreVerdict decideRestore(ResetReason r, const RtcState& s) {
  const bool ok = rtcVerify(s);
  if (r == ResetReason::DeepSleep) return ok ? RestoreVerdict::Restore : RestoreVerdict::Lost;
  const bool nonCrash = r == ResetReason::Sw || r == ResetReason::Usb || r == ResetReason::Jtag;
  if (!nonCrash && !isCrash(r)) return RestoreVerdict::Fresh;     // power-on class: RTC memory is garbage
  if (ok) {
    if (nonCrash) return RestoreVerdict::Restore;
    return s.crashCount < kCrashRestoreMax ? RestoreVerdict::RestoreCrash : RestoreVerdict::CrashLoop;
  }
  return rtcMagicOnly(s) ? RestoreVerdict::Lost : RestoreVerdict::Fresh;
}

bool isPhantomWake(WakeCause c, uint8_t woken, uint8_t lowAtBoot, int64_t lastSleepUs) {
  (void)woken;   // the S3 names the woken pin, but a tap may already be over: only "nothing LOW after a short sleep" counts
  return c == WakeCause::Ext1 && lowAtBoot == 0 && lastSleepUs < (int64_t)kPhantomMaxSleepMs * kUsPerMs;
}

bool countPhantomWake(RtcState& s, RtcUs nowRtc) {
  const bool inWindow = s.phantomWakes > 0 && nowRtc.v >= s.phantomWindowStart.v &&
                        nowRtc - s.phantomWindowStart < (int64_t)kPhantomWindowMs * kUsPerMs;
  if (!inWindow) {
    s.phantomWakes = 0;
    s.phantomWindowStart = nowRtc;
  }
  if (s.phantomWakes < 255) ++s.phantomWakes;
  return s.phantomWakes >= kPhantomWakeLimit;
}

int64_t sleptUs(const RtcState& s, RtcUs nowRtc) {
  return nowRtc.v > s.savedAt.v ? nowRtc - s.savedAt : 0;
}

}  // namespace hg
