#include "countdown.h"

namespace hg {
namespace {
int64_t presetUs(uint8_t i) { return (int64_t)Countdown::kPresetMin[i] * 60 * kUsPerS; }
}  // namespace

void Countdown::setPreset(uint8_t i) {
  if (st_ == State::Select && i < kPresetCount) preset_ = i;
}

void Countdown::nextPreset() {
  if (st_ == State::Select) preset_ = (uint8_t)((preset_ + 1) % kPresetCount);
}

uint8_t Countdown::preset() const { return preset_; }

void Countdown::start(MonoUs t) {
  if (st_ != State::Select) return;
  totalUs_ = presetUs(preset_);
  deadline_ = t + totalUs_;
  st_ = State::Running;
}

void Countdown::pause(MonoUs t) {
  if (st_ != State::Running || deadline_.v <= t.v) return;   // at/after the deadline the next tick() ends it
  pausedRemUs_ = deadline_ - t;
  st_ = State::Paused;
}

void Countdown::resume(MonoUs t) {
  if (st_ != State::Paused) return;
  deadline_ = t + pausedRemUs_;
  st_ = State::Running;
}

bool Countdown::addMinute() {
  if (st_ != State::Running || totalUs_ + 60 * kUsPerS > kMaxTotalUs) return false;
  totalUs_ += 60 * kUsPerS;
  deadline_ = deadline_ + 60 * kUsPerS;
  return true;
}

void Countdown::cancel() {
  if (st_ == State::Paused) st_ = State::Select;
}

void Countdown::restart(MonoUs t) {
  if (st_ != State::Done) return;
  st_ = State::Select;
  start(t);
}

void Countdown::toSelect() {
  if (st_ == State::Done) st_ = State::Select;
}

bool Countdown::tick(MonoUs now) {
  if (st_ != State::Running || now.v < deadline_.v) return false;
  doneAt_ = deadline_;
  st_ = State::Done;
  return true;
}

Countdown::State Countdown::state() const { return st_; }

int64_t Countdown::remainingUs(MonoUs now) const {
  switch (st_) {
    case State::Running: return deadline_.v > now.v ? deadline_ - now : 0;
    case State::Paused:  return pausedRemUs_;
    case State::Select:  return presetUs(preset_);
    default:             return 0;
  }
}

int64_t Countdown::totalUs() const { return st_ == State::Select ? presetUs(preset_) : totalUs_; }

int64_t Countdown::overtimeUs(MonoUs now) const {
  return st_ == State::Done && now.v > doneAt_.v ? now - doneAt_ : 0;
}

MonoUs Countdown::deadline() const { return deadline_; }

int64_t Countdown::displaySeconds(int64_t remainingUs) {
  return remainingUs > 0 ? (remainingUs + kUsPerS - 1) / kUsPerS : 0;
}

Countdown::Snapshot Countdown::save(MonoUs now) const {
  Snapshot s{};
  s.state = (uint8_t)st_;
  s.preset = preset_;
  s.totalUs = totalUs_;
  s.remainingUs = st_ == State::Running ? deadline_ - now : st_ == State::Paused ? pausedRemUs_ : 0;   // may be < 0
  s.overtimeUs = overtimeUs(now);
  return s;
}

void Countdown::restore(const Snapshot& s, int64_t sleptUs, MonoUs now) {
  *this = Countdown{};
  if (s.preset >= kPresetCount || s.state > (uint8_t)State::Done) return;
  preset_ = s.preset;
  if (s.state == (uint8_t)State::Select) return;
  if (s.totalUs <= 0 || s.totalUs > kMaxTotalUs) return;          // corrupt: back to Select
  const int64_t slept = sleptUs > 0 ? sleptUs : 0;
  st_ = (State)s.state;
  totalUs_ = s.totalUs;
  switch (st_) {
    case State::Running: deadline_ = now + (s.remainingUs - slept); break;   // may be past: the next tick() fires once
    case State::Paused:  pausedRemUs_ = s.remainingUs > 0 ? s.remainingUs : 1; break;
    case State::Done:    doneAt_ = MonoUs{now.v - (s.overtimeUs + slept)}; break;
    default: break;
  }
}

}  // namespace hg
