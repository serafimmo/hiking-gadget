#include "bindings.h"

#include "../config/timing.h"

namespace hg {

// Trigger rule (§2.2): no long action -> fire on Down (zero latency); with a long action -> Click + hold threshold.
KeyConfig keyConfigFor(const Binding& b) {
  if (b.longAct != ActionId::None)
    return KeyConfig{Trigger::OnClick, (uint16_t)(b.holdMs ? b.holdMs : kHoldMs), b.repeat, b.accel};
  return KeyConfig{Trigger::OnDown, 0, b.repeat, b.accel};
}

}  // namespace hg
