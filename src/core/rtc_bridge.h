#pragma once
#include "clock_policy.h"
#include "countdown.h"
#include "stopwatch.h"
namespace hg {
struct RtcState {                         // lives in RTC_NOINIT memory (hal_store), ~520 B
  uint32_t magic;                         // 0x48473252 'HG2R'
  uint16_t layout;                        // kRtcLayout = 1; any change bumps it
  uint16_t size;                          // sizeof(RtcState)
  uint32_t buildId;                       // fnv1a32 of the build stamp that wrote it ("restored across update" log when !=)
  ClockMeta clock;
  RtcUs    savedAt;                       // esp_rtc_get_time_us() when the snapshot below was taken
  Stopwatch::Snapshot sw;
  Countdown::Snapshot tm;
  int16_t  targetDeg;                     // -1 = none
  uint8_t  page;                          // carousel page
  uint8_t  alertFlags;                    // bit0 alert active, bit1 missed-alert bell
  uint8_t  lastSleep;                     // 0 none, 1 auto power-off, 2 manual
  uint8_t  stuckKeyMask;                  // keys excluded from ext1 at the last deep sleep (-> card at boot)
  uint8_t  phantomWakes;                  // phantom ext1 wakes inside the current window (§7.5)
  RtcUs    phantomWindowStart;            // RTC time of the first phantom wake of the window (2 min)
  uint8_t  crashCount;                    // consecutive crash resets without 60 s of uptime
  ShtChip  shtChip; uint8_t shtAddr;      // last detected SHT (DEEPSLEEP wakes verify it instead of a full probe, §6.2)
  RtcUs    shtBusyUntil;                  // RTC time until which a heater pulse may still run (heater ACK + 1.2 s); written at the ACK
  uint32_t crc;                           // crc32 of all bytes before this field
};
constexpr uint32_t kRtcMagic = 0x48473252;
constexpr uint16_t kRtcLayout = 1;
void rtcSeal(RtcState& s);                // fills magic/layout/size/crc
bool rtcVerify(const RtcState& s);        // magic, layout, size and crc all match
bool rtcMagicOnly(const RtcState& s);     // magic matches but layout/size/crc do not: a snapshot existed and is unusable
enum class RestoreVerdict : uint8_t { Fresh, Restore, RestoreCrash, Lost, CrashLoop };
RestoreVerdict decideRestore(ResetReason r, const RtcState& s);  // §5.4 table (uses rtcVerify/rtcMagicOnly/crashCount)
bool isPhantomWake(WakeCause c, uint8_t woken, uint8_t lowAtBoot, int64_t lastSleepUs);  // §7.5
int64_t sleptUs(const RtcState& s, RtcUs nowRtc);  // nowRtc - savedAt (real time between the snapshot and now), clamped to >= 0
// Counts one phantom wake in s.phantomWakes inside a kPhantomWindowMs window that starts at the first one (a wake
// outside the window restarts the count); true when kPhantomWakeLimit is reached -> exclude the woken pins (§7.5).
bool countPhantomWake(RtcState& s, RtcUs nowRtc);
}  // namespace hg
