#include "build_checks.h"
#include "hal_ports.h"

#include "../config/board_pins.h"

namespace hg {
namespace {
volatile uint32_t s_edges = 0;
void IRAM_ATTR onEdge() { s_edges = s_edges + 1; }
bool s_attached = false;
}  // namespace

// Diagnostics only: the one ISR of the firmware, attached while Diagnostics > Sensors is open (§4.2 rule 4).
void HalDrdy::enable(DrdyKind k) {
  disable();
  if (k == DrdyKind::None) { pinMode(kPinMagDrdy, INPUT_PULLDOWN); return; }
  pinMode(kPinMagDrdy, INPUT);
  attachInterrupt(digitalPinToInterrupt(kPinMagDrdy), onEdge, k == DrdyKind::RisingLevel ? RISING : FALLING);
  s_attached = true;
}

void HalDrdy::disable() {
  if (s_attached) detachInterrupt(digitalPinToInterrupt(kPinMagDrdy));
  s_attached = false;
}

uint32_t HalDrdy::edges() const { return s_edges; }

}  // namespace hg
