#include "time_service.h"

#include <stdio.h>

#include "../config/build_config.h"
#include "../config/timing.h"
#include "../core/crc.h"

namespace hg {

BootClockOut TimeService::boot(const char* buildDate, const char* buildTime, bool rtcValid, const ClockMeta& rtcMeta) {
  BootClockIn in{};
  in.rtcStateValid = rtcValid;
  in.rtcMeta = rtcMeta;
  UtcMs sys{0};
  in.sysTimeValid = wall_ && wall_->get(&sys) && sys.v / 1000 >= kMinValidUtcS && sys.v / 1000 < kMaxValidUtcS;
  in.sysUtcS = sys.v / 1000;
  in.nvsValid = persist_.loadClk(in.nvsUtcS, in.nvsMeta);
  Civil b{};
  if (buildDate && buildTime && parseBuildStamp(buildDate, buildTime, b)) {
    in.buildValid = true;
    in.buildUtcS = epochFromCivil(b) - (int64_t)kBuildTzOffsetMin * 60 + kBuildToBootLagS;
    char stamp[32];
    snprintf(stamp, sizeof stamp, "%s %s", buildDate, buildTime);
    in.buildId = fnv1a32(stamp);
  }
  buildUtcS_ = in.buildValid ? in.buildUtcS : 0;
  buildId_ = in.buildId;
  in.nvsBuildId = persist_.loadBuildId();
  const BootClockOut out = decideBootClock(in);
  meta_ = out.meta;
  if (out.setTime && wall_) wall_->set(UtcMs{out.utcS * 1000});
  if (out.storeBuildId && !persist_.saveBuildId(in.buildId))
    log_.log(LogLevel::E, "NVS", "E-NVS put 'bld' failed");
  return out;
}

bool TimeService::valid() const { return meta_.q != ClockQuality::None && utcS() > 0; }

int64_t TimeService::utcMs() const {
  if (meta_.q == ClockQuality::None || !wall_) return 0;
  UtcMs t{0};
  return wall_->get(&t) ? t.v : 0;
}

int64_t TimeService::utcS() const { return utcMs() / 1000; }

int64_t TimeService::syncAgeS() const {
  if (!meta_.lastSyncUtcS) return 0;
  const int64_t a = utcS() - meta_.lastSyncUtcS;
  return a > 0 ? a : 0;
}

SyncVerdict TimeService::apply(int64_t unixMs, ClockQuality q, ClockSource src, int32_t* deltaMs) {
  const SyncVerdict v = validateSync(unixMs / 1000, buildUtcS_);
  if (v != SyncVerdict::Ok) return v;
  const int64_t before = utcMs();
  const int64_t d = before ? before - unixMs : 0;
  const int32_t d32 = (int32_t)(d > 2000000000 ? 2000000000 : d < -2000000000 ? -2000000000 : d);
  if (deltaMs) *deltaMs = d32;
  if (wall_) wall_->set(UtcMs{unixMs});
  meta_ = ClockMeta{q, src, unixMs / 1000, d32};
  checkpoint();
  return v;
}

SyncVerdict TimeService::syncUnixMs(int64_t unixMs, int32_t* deltaMs) {
  return apply(unixMs, ClockQuality::Synced, ClockSource::Serial, deltaMs);
}

SyncVerdict TimeService::setLocal(const Civil& local, const TzDef& tz, ClockSource src, int32_t* deltaMs) {
  return apply(localToUtc(local, tz) * 1000, ClockQuality::Manual, src, deltaMs);
}

bool TimeService::checkpoint() {
  if (!valid()) return false;
  if (!persist_.saveClk(utcS(), meta_)) {
    log_.log(LogLevel::E, "NVS", "E-NVS put 'clk' failed");
    return false;
  }
  return true;
}

void TimeService::tick(MonoUs now) {
  if (!haveCheckpointTime_) { lastCheckpoint_ = now; haveCheckpointTime_ = true; return; }
  if (now - lastCheckpoint_ >= (int64_t)kClockCheckpointMs * kUsPerMs) {
    lastCheckpoint_ = now;
    checkpoint();
  }
}

}  // namespace hg
