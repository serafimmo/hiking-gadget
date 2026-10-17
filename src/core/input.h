#pragma once
#include "types.h"
namespace hg {
enum class Trigger : uint8_t { OnDown, OnClick };
struct KeyConfig { Trigger shortTrigger; uint16_t holdMs; bool repeat; bool accel; };   // holdMs 0 = no long action
struct InputEvent {
  enum class Type : uint8_t { Down, Short, Long, Repeat, Up, Stuck, Unstuck } type;
  Key key; MonoUs tDown; uint16_t heldMs; uint8_t step;   // step: 1, or 5 when accelerated
};
struct HoldState { bool active; Key key; float progress; };   // progress 0..1 from kHoldVisualMs to the latched holdMs
// Semantics: SPEC §2.2. The KeyConfig is latched at Down (D25). Short for an OnDown press is emitted immediately
// after Down with the same tDown; such a press then emits only Repeat (if its latched config has repeat) and Up.
// Up is emitted for every accepted press that was not swallowed. Stuck/Unstuck (key set, other fields 0) are
// emitted once when a key enters or leaves stuckMask() (D26); a stuck key's press emits no Up.
class InputLogic {
 public:
  void configure(Key k, const KeyConfig& c);      // from the current Bindings, before feed(); applies to the NEXT press of k only
  void setCaptureAll(bool on);                    // Diagnostics > Buttons: every key reported independently; no stuck rule
  void feed(uint8_t rawPressedMask, MonoUs now);  // bit i = Key(i) electrically pressed
  bool pop(InputEvent& out);                      // FIFO of 16; overflow drops the oldest and counts
  void swallowUntilRelease(uint8_t keyMask);      // keys of the mask that are down now: no events until released; keys already up: no effect
  HoldState hold(MonoUs now) const;
  uint8_t  downMask() const;                      // debounced, stuck keys excluded
  uint8_t  stuckMask() const;                     // keys LOW >= kKeyStuckMs (§2.2); a key leaves it after kKeyStuckClearMs HIGH
  uint32_t dropped() const;

 private:
  static constexpr uint8_t kQueue = 16;
  struct KeyState {
    bool raw = false;            // last sample (true = LOW = pressed)
    bool down = false;           // debounced level
    MonoUs runStart{0};          // first sample of the current run of raw != down
    uint8_t runSamples = 0;
    MonoUs tDown{0};             // first LOW sample of the debounced press
    MonoUs tUp{0};               // first HIGH sample of the debounced release
    bool accepted = false;       // this press owns events (not ignored, not swallowed, not stuck)
    bool swallow = false;        // swallowUntilRelease() for the current/next press
    bool longFired = false;
    bool stuck = false;
    MonoUs nextRepeat{0};
    KeyConfig cfg{Trigger::OnDown, 0, false, false};      // configured for the next press
    KeyConfig latched{Trigger::OnDown, 0, false, false};  // latched at Down (D25)
  };
  void debounce(uint8_t i, bool rawLow, MonoUs now);
  void onDown(uint8_t i);
  void onUp(uint8_t i);
  void whileHeld(uint8_t i, MonoUs now);
  void push(InputEvent::Type t, uint8_t i, MonoUs tDown, int64_t heldUs, uint8_t step);
  KeyState k_[kKeyCount];
  bool captureAll_ = false;
  InputEvent q_[kQueue] = {};
  uint8_t qHead_ = 0, qCount_ = 0;
  uint32_t dropped_ = 0;
};
}  // namespace hg
