#include "clock_policy.h"

namespace hg {
namespace {
bool inRange(int64_t s) { return s >= kMinValidUtcS && s < kMaxValidUtcS; }
}  // namespace

BootClockOut decideBootClock(const BootClockIn& in) {
  BootClockOut out{};
  const bool buildCand = in.buildValid && inRange(in.buildUtcS) && in.buildId != in.nvsBuildId;
  const bool nvsCand = in.nvsValid && inRange(in.nvsUtcS);
  // The build stamp is consumed whenever it is a candidate, even if a better source wins: stored, it can never bring
  // back a stale time on a later power-on (§5.2).
  out.storeBuildId = buildCand;
  const bool sysOk = in.sysTimeValid && inRange(in.sysUtcS);
  if (sysOk) {                                             // rows 1 and 2: the system time survived
    out.utcS = in.sysUtcS;
    if (in.rtcStateValid) out.meta = in.rtcMeta;
    else out.meta = ClockMeta{ClockQuality::Approx, ClockSource::Restored, in.nvsValid ? in.nvsMeta.lastSyncUtcS : 0,
                              in.nvsValid ? in.nvsMeta.lastSyncDeltaMs : 0};
    return out;
  }
  const ClockMeta keep = in.nvsValid ? in.nvsMeta : ClockMeta{};
  if (buildCand || nvsCand) {                              // row 3: newest candidate
    const bool useBuild = buildCand && (!nvsCand || in.buildUtcS >= in.nvsUtcS);
    out.setTime = true;
    out.utcS = useBuild ? in.buildUtcS : in.nvsUtcS;
    out.meta = ClockMeta{ClockQuality::Approx, useBuild ? ClockSource::Build : ClockSource::Restored, keep.lastSyncUtcS,
                         keep.lastSyncDeltaMs};
    out.clockLost = !useBuild;
    return out;
  }
  out.meta = ClockMeta{ClockQuality::None, ClockSource::None, keep.lastSyncUtcS, keep.lastSyncDeltaMs};   // row 4
  out.clockLost = true;
  return out;
}

SyncVerdict validateSync(int64_t utcS, int64_t buildUtcS) {
  if (utcS < kMinValidUtcS) return SyncVerdict::BeforeMin;
  if (utcS >= kMaxValidUtcS) return SyncVerdict::AfterMax;
  if (buildUtcS > 0 && utcS < buildUtcS - 86400) return SyncVerdict::BeforeBuild;
  return SyncVerdict::Ok;
}

}  // namespace hg
