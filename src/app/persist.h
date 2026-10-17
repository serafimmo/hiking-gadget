#pragma once
// NVS keys of namespace hg2 over IKvStore (SPEC §8.2): one key per setting, blobs with magic + version + CRC32.
#include "../core/clock_policy.h"
#include "../core/compass_math.h"
#include "../core/settings.h"
#include "ports.h"

namespace hg {
class Persist {
 public:
  explicit Persist(IKvStore* kv) : kv_(kv) {}
  bool ok() const { return kv_ && kv_->ok(); }
  struct LoadResult { bool anyInvalid; SettingId firstInvalid; int16_t firstOld; uint8_t invalidCount; };
  LoadResult loadSettings(Settings& s);      // missing -> default; invalid -> sanitized, the first one reported
  bool saveSetting(SettingId id, int16_t v); // the time zone is stored as its stable id string
  bool resetSettings();                      // erases the 13 setting keys + tmpre; keeps cal, clk, bld
  bool factory();
  uint8_t loadPreset();                      // default 2 (5 min)
  bool savePreset(uint8_t p);
  bool loadCal(MagCal& c, int64_t* savedUtcS);
  bool saveCal(const MagCal& c, int64_t savedUtcS);
  bool loadClk(int64_t& utcS, ClockMeta& m);
  bool saveClk(int64_t utcS, const ClockMeta& m);
  uint32_t loadBuildId();
  bool saveBuildId(uint32_t id);
  uint32_t bumpBoots();
  void pushResetReason(uint8_t r, uint8_t ring[8]);
  uint32_t putFailures() const { return putFail_; }
  const char* lastFailedKey() const { return lastFailKey_; }

 private:
  bool put(const char* key, const void* buf, size_t len);
  IKvStore* kv_;
  uint32_t putFail_ = 0;
  const char* lastFailKey_ = "";
};
}  // namespace hg
