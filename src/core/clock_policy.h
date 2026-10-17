#pragma once
#include "types.h"
namespace hg {
enum class ClockQuality : uint8_t { None, Approx, Manual, Synced };
enum class ClockSource  : uint8_t { None, Build, Restored, Manual, Serial };
struct ClockMeta { ClockQuality q; ClockSource src; int64_t lastSyncUtcS; int32_t lastSyncDeltaMs; };
constexpr int64_t kMinValidUtcS = 1735689600;   // 2025-01-01T00:00:00Z
constexpr int64_t kMaxValidUtcS = 4102444800;   // 2100-01-01T00:00:00Z
struct BootClockIn {
  bool rtcStateValid; ClockMeta rtcMeta;         // RtcState verified (deep sleep / SW reset survived)
  bool sysTimeValid;  int64_t sysUtcS;           // gettimeofday() within [kMin, kMax)
  bool nvsValid;      int64_t nvsUtcS; ClockMeta nvsMeta;   // NVS 'clk' checkpoint
  bool buildValid;    int64_t buildUtcS;         // parsed __DATE__/__TIME__ in kBuildTzOffsetMin + kBuildToBootLagS
  uint32_t buildId;   uint32_t nvsBuildId;       // fnv1a32 of "__DATE__ __TIME__"; NVS 'bld' (0 if absent)
};
struct BootClockOut { bool setTime; int64_t utcS; ClockMeta meta; bool storeBuildId; bool clockLost; };
BootClockOut decideBootClock(const BootClockIn& in);     // table in §5.3
enum class SyncVerdict : uint8_t { Ok, BeforeMin, AfterMax, BeforeBuild };
SyncVerdict validateSync(int64_t utcS, int64_t buildUtcS); // BeforeBuild if utc < build - 1 day
}  // namespace hg
