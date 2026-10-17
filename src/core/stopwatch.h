#pragma once
#include "types.h"
namespace hg {
class Stopwatch {
 public:
  enum class State : uint8_t { Idle, Running, Paused };
  static constexpr uint8_t kMaxLaps = 99;                         // ring of the most recent splits
  static constexpr uint16_t kMaxLapNo = 999;                      // lap numbers shown in a 3-digit cell
  static constexpr int64_t kMaxElapsedUs = 360000LL * kUsPerS - 10000; // 99:59:59.99
  void start(MonoUs t);  void pause(MonoUs t);  void resume(MonoUs t);  void reset();
  bool lap(MonoUs t);                                 // Running and lapNo < 999 only; false otherwise (-> toast "Max 999 laps")
  bool tick(MonoUs now);                              // true once when the cap auto-pauses
  State    state() const;
  int64_t  elapsedUs(MonoUs now) const;
  int64_t  currentLapUs(MonoUs now) const;            // since the last split
  uint16_t lapNo() const;                             // completed laps, keeps counting past 99 up to 999
  uint8_t  storedLaps() const;                        // min(lapNo, 99)
  int64_t  lapUs(uint8_t fromNewest) const;           // duration of a completed lap (0 = newest)
  int64_t  lapDeltaUs(uint8_t fromNewest) const;      // lapUs(i) - lapUs(i+1); 0 if no older lap
  // lapMs: ring of completed lap durations (ms, newest at head-1); lastSplitUs: elapsed at the last lap press, which
  // cannot be rebuilt from the ring once more than 99 laps were taken.
  struct Snapshot { uint8_t state; uint8_t head; uint16_t lapNo; int64_t elapsedUs; int64_t lastSplitUs; uint32_t lapMs[kMaxLaps]; };
  Snapshot save(MonoUs now) const;                    // elapsed evaluated at 'now'
  void restore(const Snapshot& s, int64_t sleptUs, MonoUs now);  // Running: elapsed = s.elapsed + sleptUs, keeps running from now

 private:
  State    st_ = State::Idle;
  int64_t  accUs_ = 0;          // elapsed accumulated before runStart_
  MonoUs   runStart_{0};        // valid while Running
  int64_t  lastSplitUs_ = 0;    // elapsed at the last lap press
  uint16_t lapNo_ = 0;
  uint8_t  head_ = 0;           // ring index of the next lap to write
  uint32_t lapMs_[kMaxLaps] = {};  // completed lap durations in ms (ring)
};
}  // namespace hg
