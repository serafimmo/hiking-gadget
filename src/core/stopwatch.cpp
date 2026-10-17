#include "stopwatch.h"

namespace hg {

void Stopwatch::start(MonoUs t) {
  if (st_ != State::Idle) return;
  reset();
  st_ = State::Running;
  runStart_ = t;
}

void Stopwatch::pause(MonoUs t) {
  if (st_ != State::Running) return;
  accUs_ = elapsedUs(t);
  st_ = State::Paused;
}

void Stopwatch::resume(MonoUs t) {
  if (st_ != State::Paused || accUs_ >= kMaxElapsedUs) return;
  st_ = State::Running;
  runStart_ = t;
}

void Stopwatch::reset() { *this = Stopwatch{}; }

bool Stopwatch::lap(MonoUs t) {
  if (st_ != State::Running || lapNo_ >= kMaxLapNo) return false;
  const int64_t split = elapsedUs(t);
  const int64_t lapUs = split - lastSplitUs_;
  lapMs_[head_] = (uint32_t)((lapUs + 500) / 1000);
  head_ = (uint8_t)((head_ + 1) % kMaxLaps);
  lastSplitUs_ = split;
  ++lapNo_;
  return true;
}

bool Stopwatch::tick(MonoUs now) {
  if (st_ != State::Running || elapsedUs(now) < kMaxElapsedUs) return false;
  accUs_ = kMaxElapsedUs;
  st_ = State::Paused;
  return true;
}

Stopwatch::State Stopwatch::state() const { return st_; }

int64_t Stopwatch::elapsedUs(MonoUs now) const {
  int64_t e = accUs_;
  if (st_ == State::Running && now.v > runStart_.v) e += now - runStart_;
  return e < kMaxElapsedUs ? e : kMaxElapsedUs;
}

int64_t Stopwatch::currentLapUs(MonoUs now) const { return elapsedUs(now) - lastSplitUs_; }

uint16_t Stopwatch::lapNo() const { return lapNo_; }

uint8_t Stopwatch::storedLaps() const { return (uint8_t)(lapNo_ < kMaxLaps ? lapNo_ : kMaxLaps); }

int64_t Stopwatch::lapUs(uint8_t fromNewest) const {
  if (fromNewest >= storedLaps()) return 0;
  const int idx = ((int)head_ - 1 - (int)fromNewest + 2 * kMaxLaps) % kMaxLaps;
  return (int64_t)lapMs_[idx] * kUsPerMs;
}

int64_t Stopwatch::lapDeltaUs(uint8_t fromNewest) const {
  if (fromNewest + 1 >= storedLaps()) return 0;
  return lapUs(fromNewest) - lapUs((uint8_t)(fromNewest + 1));
}

Stopwatch::Snapshot Stopwatch::save(MonoUs now) const {
  Snapshot s{};
  s.state = (uint8_t)st_;
  s.head = head_;
  s.lapNo = lapNo_;
  s.elapsedUs = elapsedUs(now);
  s.lastSplitUs = lastSplitUs_;
  for (int i = 0; i < kMaxLaps; ++i) s.lapMs[i] = lapMs_[i];
  return s;
}

void Stopwatch::restore(const Snapshot& s, int64_t sleptUs, MonoUs now) {
  reset();
  if (s.state > (uint8_t)State::Paused || s.head >= kMaxLaps || s.lapNo > kMaxLapNo || s.elapsedUs < 0) return;
  st_ = (State)s.state;
  head_ = s.head;
  lapNo_ = s.lapNo;
  lastSplitUs_ = s.lastSplitUs;
  for (int i = 0; i < kMaxLaps; ++i) lapMs_[i] = s.lapMs[i];
  int64_t e = s.elapsedUs + (st_ == State::Running && sleptUs > 0 ? sleptUs : 0);
  accUs_ = e < kMaxElapsedUs ? e : kMaxElapsedUs;
  runStart_ = now;
  if (st_ == State::Idle) accUs_ = 0;
}

}  // namespace hg
