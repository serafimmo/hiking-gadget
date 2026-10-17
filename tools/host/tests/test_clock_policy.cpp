// core/clock_policy: every row of §5.3, validateSync boundaries, the build-id gate.
#include "core/clock_policy.h"
#include "tiny_test.h"

using namespace hg;

namespace {
constexpr int64_t kBuild = 1791736242, kNvs = 1791000000, kSys = 1791800000;
BootClockIn base() {
  BootClockIn in{};
  in.buildValid = true; in.buildUtcS = kBuild; in.buildId = 0x1880C947; in.nvsBuildId = 0;
  return in;
}
}  // namespace

TEST(clock_row1_rtc_and_sys_valid_keeps_time_and_meta) {
  BootClockIn in = base();
  in.rtcStateValid = true; in.rtcMeta = ClockMeta{ClockQuality::Synced, ClockSource::Serial, kSys - 100, -1840};
  in.sysTimeValid = true; in.sysUtcS = kSys;
  const BootClockOut o = decideBootClock(in);
  CHECK(!o.setTime); CHECK(!o.clockLost);
  CHECK_EQ(o.utcS, kSys);
  CHECK_EQ(o.meta.q, ClockQuality::Synced); CHECK_EQ(o.meta.src, ClockSource::Serial);
  CHECK_EQ(o.meta.lastSyncDeltaMs, -1840);
  CHECK(o.storeBuildId);              // the new build's stamp is consumed, never used later
}

TEST(clock_row2_sys_valid_without_rtc_state_is_approx_restored) {
  BootClockIn in = base();
  in.sysTimeValid = true; in.sysUtcS = kSys;
  const BootClockOut o = decideBootClock(in);
  CHECK(!o.setTime); CHECK(!o.clockLost);
  CHECK_EQ(o.meta.q, ClockQuality::Approx); CHECK_EQ(o.meta.src, ClockSource::Restored);
}

TEST(clock_row3_newest_candidate_wins) {
  BootClockIn in = base();                                   // build only
  BootClockOut o = decideBootClock(in);
  CHECK(o.setTime); CHECK_EQ(o.utcS, kBuild); CHECK_EQ(o.meta.src, ClockSource::Build);
  CHECK_EQ(o.meta.q, ClockQuality::Approx); CHECK(o.storeBuildId); CHECK(!o.clockLost);

  in.nvsValid = true; in.nvsUtcS = kNvs;                     // build newer than NVS
  o = decideBootClock(in);
  CHECK_EQ(o.utcS, kBuild); CHECK_EQ(o.meta.src, ClockSource::Build); CHECK(!o.clockLost);

  in.nvsUtcS = kBuild + 3600;                                // NVS newer
  in.nvsMeta = ClockMeta{ClockQuality::Synced, ClockSource::Serial, kBuild + 10, 250};
  o = decideBootClock(in);
  CHECK_EQ(o.utcS, kBuild + 3600); CHECK_EQ(o.meta.src, ClockSource::Restored);
  CHECK_EQ(o.meta.q, ClockQuality::Approx); CHECK(o.clockLost); CHECK(o.storeBuildId);
  CHECK_EQ(o.meta.lastSyncUtcS, kBuild + 10);                // the last sync is remembered for the age display
}

TEST(clock_build_id_gate) {
  BootClockIn in = base();
  in.nvsBuildId = in.buildId;                                // same firmware rebooted: no build candidate
  BootClockOut o = decideBootClock(in);
  CHECK(!o.setTime); CHECK(!o.storeBuildId);
  CHECK_EQ(o.meta.q, ClockQuality::None); CHECK(o.clockLost);           // row 4
  in.nvsValid = true; in.nvsUtcS = kNvs;
  o = decideBootClock(in);
  CHECK(o.setTime); CHECK_EQ(o.utcS, kNvs); CHECK_EQ(o.meta.src, ClockSource::Restored); CHECK(!o.storeBuildId);
}

TEST(clock_row4_no_candidates) {
  BootClockIn in{};
  const BootClockOut o = decideBootClock(in);
  CHECK(!o.setTime); CHECK(o.clockLost); CHECK_EQ(o.meta.q, ClockQuality::None); CHECK(!o.storeBuildId);
  BootClockIn bad = base(); bad.buildUtcS = 1000;            // out-of-range build stamp is not a candidate
  CHECK(!decideBootClock(bad).setTime);
}

TEST(clock_validate_sync_boundaries) {
  CHECK_EQ(validateSync(kMinValidUtcS - 1, 0), SyncVerdict::BeforeMin);
  CHECK_EQ(validateSync(kMinValidUtcS, 0), SyncVerdict::Ok);
  CHECK_EQ(validateSync(kMaxValidUtcS - 1, 0), SyncVerdict::Ok);
  CHECK_EQ(validateSync(kMaxValidUtcS, 0), SyncVerdict::AfterMax);
  CHECK_EQ(validateSync(kBuild - 86400 - 1, kBuild), SyncVerdict::BeforeBuild);
  CHECK_EQ(validateSync(kBuild - 86400, kBuild), SyncVerdict::Ok);
}
