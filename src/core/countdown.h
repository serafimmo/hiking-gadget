#pragma once
#include "types.h"
namespace hg {
class Countdown {
 public:
  enum class State : uint8_t { Select, Running, Paused, Done };
  static constexpr uint8_t  kPresetCount = 6;
  static constexpr uint16_t kPresetMin[kPresetCount] = {1, 3, 5, 10, 30, 60};
  static constexpr int64_t  kMaxTotalUs = 10LL * 3600 * kUsPerS;
  void setPreset(uint8_t i);  void nextPreset();  uint8_t preset() const;
  void start(MonoUs t);  void pause(MonoUs t);  void resume(MonoUs t);
  bool addMinute();                        // Running only: deadline and total += 60 s; false (no change) if total would exceed kMaxTotalUs -> toast "Max 10 h"
  void cancel();                           // Paused -> Select (preset kept)
  void restart(MonoUs t);                  // Done -> Running, same preset
  void toSelect();                         // Done -> Select
  bool tick(MonoUs now);                   // true exactly once on Running -> Done (doneAt = deadline)
  State   state() const;
  int64_t remainingUs(MonoUs now) const;   // Running/Paused
  int64_t totalUs() const;
  int64_t overtimeUs(MonoUs now) const;    // Done: now - doneAt
  MonoUs  deadline() const;                // Running only
  static int64_t displaySeconds(int64_t remainingUs);   // ceil(remaining / 1 s)
  struct Snapshot { uint8_t state, preset; int64_t totalUs, remainingUs, overtimeUs; };
  Snapshot save(MonoUs now) const;
  void restore(const Snapshot& s, int64_t sleptUs, MonoUs now);  // Running: deadline = now + remaining - slept (may be past -> next tick() fires)

 private:
  State   st_ = State::Select;
  uint8_t preset_ = 0;
  int64_t totalUs_ = 0;
  MonoUs  deadline_{0};         // Running
  int64_t pausedRemUs_ = 0;     // Paused
  MonoUs  doneAt_{0};           // Done
};
}  // namespace hg
