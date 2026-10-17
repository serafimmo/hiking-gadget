#include "input.h"

#include "../config/timing.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }
}  // namespace

void InputLogic::configure(Key k, const KeyConfig& c) { k_[(uint8_t)k].cfg = c; }

void InputLogic::setCaptureAll(bool on) { captureAll_ = on; }

void InputLogic::push(InputEvent::Type t, uint8_t i, MonoUs tDown, int64_t heldUs, uint8_t step) {
  InputEvent e{};
  e.type = t;
  e.key = (Key)i;
  e.tDown = tDown;
  const int64_t heldMs = heldUs / kUsPerMs;
  e.heldMs = (uint16_t)(heldMs < 0 ? 0 : heldMs > 65535 ? 65535 : heldMs);
  e.step = step;
  if (qCount_ == kQueue) {               // drop the oldest, count it
    qHead_ = (uint8_t)((qHead_ + 1) % kQueue);
    --qCount_;
    ++dropped_;
  }
  q_[(qHead_ + qCount_) % kQueue] = e;
  ++qCount_;
}

bool InputLogic::pop(InputEvent& out) {
  if (!qCount_) return false;
  out = q_[qHead_];
  qHead_ = (uint8_t)((qHead_ + 1) % kQueue);
  --qCount_;
  return true;
}

void InputLogic::onDown(uint8_t i) {
  KeyState& k = k_[i];
  k.latched = k.cfg;
  k.longFired = false;
  k.accepted = false;
  if (k.stuck) return;
  if (k.swallow) return;                                         // swallowed until release
  if (!captureAll_ && (downMask() & ~(1u << i)) != 0) return;   // the first key down owns the gesture
  k.accepted = true;
  push(InputEvent::Type::Down, i, k.tDown, 0, 0);
  if (k.latched.shortTrigger == Trigger::OnDown) push(InputEvent::Type::Short, i, k.tDown, 0, 1);
  k.nextRepeat = k.tDown + ms(kRepeatDelayMs);
}

void InputLogic::onUp(uint8_t i) {
  KeyState& k = k_[i];
  const bool wasAccepted = k.accepted && !k.stuck;
  k.accepted = false;
  k.swallow = false;
  if (!wasAccepted) return;
  const int64_t held = k.tUp - k.tDown;
  if (k.latched.shortTrigger == Trigger::OnClick && !k.longFired && held < ms(kClickMaxMs))
    push(InputEvent::Type::Short, i, k.tDown, held, 1);         // Click; 400 ms..hold = hesitation zone: nothing
  push(InputEvent::Type::Up, i, k.tDown, held, 0);
}

void InputLogic::whileHeld(uint8_t i, MonoUs now) {
  KeyState& k = k_[i];
  const int64_t held = now - k.tDown;
  if (!captureAll_ && !k.stuck && held >= ms(kKeyStuckMs)) {       // D26: loses ownership, emits nothing more
    k.stuck = true;
    k.accepted = false;
    push(InputEvent::Type::Stuck, i, MonoUs{0}, 0, 0);
    return;
  }
  if (!k.accepted) return;
  if (k.latched.shortTrigger == Trigger::OnClick && k.latched.holdMs && !k.longFired && held >= ms(k.latched.holdMs)) {
    k.longFired = true;
    push(InputEvent::Type::Long, i, k.tDown, held, 0);
  }
  if (k.latched.repeat && !k.longFired && now.v >= k.nextRepeat.v) {
    const uint8_t step = (k.latched.accel && held >= ms(kAccelAfterMs)) ? kAccelStep : 1;
    push(InputEvent::Type::Repeat, i, k.tDown, held, step);
    const int64_t period = ms(held >= ms(kRepeatFastAfterMs) ? kRepeatFastMs : kRepeatMs);
    k.nextRepeat = k.nextRepeat + period;
    if (k.nextRepeat.v <= now.v) k.nextRepeat = now + period;    // a late loop does not cause a burst
  }
}

void InputLogic::debounce(uint8_t i, bool rawLow, MonoUs now) {
  KeyState& k = k_[i];
  if (rawLow != k.raw) {                                           // a new run starts with this sample
    k.raw = rawLow;
    k.runStart = now;
    k.runSamples = 0;
  }
  if (k.raw == k.down) {
    k.runSamples = 0;
    if (!k.down && k.swallow && !k.accepted) k.swallow = false;   // a swallow never outlives a press that did not happen
    return;
  }
  if (k.runSamples < 255) ++k.runSamples;
  if (k.runSamples < 2 || now - k.runStart < ms(kDebounceMs)) return;
  k.down = k.raw;
  if (k.down) {
    k.tDown = k.runStart;
    onDown(i);
  } else {
    k.tUp = k.runStart;
    onUp(i);
  }
}

void InputLogic::feed(uint8_t rawPressedMask, MonoUs now) {
  for (uint8_t i = 0; i < kKeyCount; ++i) {
    KeyState& k = k_[i];
    const bool wasDown = k.down;
    debounce(i, (rawPressedMask >> i) & 1u, now);
    if (k.down) {
      whileHeld(i, now);
    } else if (k.stuck && !wasDown) {
      if (now - k.tUp >= ms(kKeyStuckClearMs)) {                   // HIGH for 1 s: the key works again
        k.stuck = false;
        push(InputEvent::Type::Unstuck, i, MonoUs{0}, 0, 0);
      }
    }
  }
}

void InputLogic::swallowUntilRelease(uint8_t keyMask) {
  for (uint8_t i = 0; i < kKeyCount; ++i) {
    if (!((keyMask >> i) & 1u)) continue;
    KeyState& k = k_[i];
    if (!k.down && !k.raw) continue;                               // already up: no effect
    k.swallow = true;
    if (k.accepted) {
      k.accepted = false;
      // Drop the events of this press that are still queued (e.g. the Short of an OnDown press).
      uint8_t n = 0;
      for (uint8_t j = 0; j < qCount_; ++j) {
        const InputEvent& e = q_[(qHead_ + j) % kQueue];
        if ((uint8_t)e.key == i && e.tDown == k.tDown && e.type != InputEvent::Type::Stuck &&
            e.type != InputEvent::Type::Unstuck)
          continue;
        q_[(qHead_ + n) % kQueue] = e;
        ++n;
      }
      qCount_ = n;
    }
  }
}

HoldState InputLogic::hold(MonoUs now) const {
  for (uint8_t i = 0; i < kKeyCount; ++i) {
    const KeyState& k = k_[i];
    if (!k.down || !k.accepted || k.latched.shortTrigger != Trigger::OnClick || !k.latched.holdMs) continue;
    const int64_t held = now - k.tDown;
    if (held < ms(kHoldVisualMs)) continue;
    float p = k.longFired ? 1.0f
                          : (float)(held - ms(kHoldVisualMs)) / (float)(ms(k.latched.holdMs) - ms(kHoldVisualMs));
    if (p > 1.0f) p = 1.0f;
    return HoldState{true, (Key)i, p};
  }
  return HoldState{false, Key::Up, 0.0f};
}

uint8_t InputLogic::downMask() const {
  uint8_t m = 0;
  for (uint8_t i = 0; i < kKeyCount; ++i)
    if (k_[i].down && !k_[i].stuck) m |= (uint8_t)(1u << i);
  return m;
}

uint8_t InputLogic::stuckMask() const {
  uint8_t m = 0;
  for (uint8_t i = 0; i < kKeyCount; ++i)
    if (k_[i].stuck) m |= (uint8_t)(1u << i);
  return m;
}

uint32_t InputLogic::dropped() const { return dropped_; }

}  // namespace hg
