#pragma once
// Wall clock + quality + sync + checkpoint (SPEC §5). The wall clock is read and set only through IWallClock;
// intervals never use it (§5.1).
#include "../core/clock_policy.h"
#include "../core/tz.h"
#include "logger.h"
#include "persist.h"

namespace hg {
class TimeService {
 public:
  TimeService(IWallClock* wall, Persist& persist, Logger& log) : wall_(wall), persist_(persist), log_(log) {}
  // §5.3; returns the decision (the app shows toasts/cards for clockLost and Approx).
  BootClockOut boot(const char* buildDate, const char* buildTime, bool rtcValid, const ClockMeta& rtcMeta);
  bool valid() const;
  int64_t utcS() const;                        // 0 when not valid
  int64_t utcMs() const;
  const ClockMeta& meta() const { return meta_; }
  int64_t buildUtcS() const { return buildUtcS_; }
  uint32_t buildId() const { return buildId_; }
  // Serial TIME <unix>[.ms]: Synced. delta = device minus PC before the set.
  SyncVerdict syncUnixMs(int64_t unixMs, int32_t* deltaMs);
  // TIME YYYY-MM-DD HH:MM[:SS] (q Manual, src Serial) or Settings > Set time (q Manual, src Manual).
  SyncVerdict setLocal(const Civil& local, const TzDef& tz, ClockSource src, int32_t* deltaMs);
  void tick(MonoUs now);                       // hourly NVS checkpoint while awake
  bool checkpoint();                           // NVS 'clk' (on sync/set, hourly, before deep sleep)
  int64_t syncAgeS() const;

 private:
  SyncVerdict apply(int64_t unixMs, ClockQuality q, ClockSource src, int32_t* deltaMs);
  IWallClock* wall_;
  Persist& persist_;
  Logger& log_;
  ClockMeta meta_{ClockQuality::None, ClockSource::None, 0, 0};
  int64_t buildUtcS_ = 0;
  uint32_t buildId_ = 0;
  MonoUs lastCheckpoint_{0};
  bool haveCheckpointTime_ = false;
};
}  // namespace hg
