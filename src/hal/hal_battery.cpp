#include "build_checks.h"
#include "hal_ports.h"

#include "../config/board_pins.h"

namespace hg {

// Stub unless kPinBatteryAdc >= 0 (§6.5).
BatteryState HalBattery::read() {
  if (kPinBatteryAdc < 0) return BatteryState{false, 0, 0};
  uint32_t sum = 0;
  for (int i = 0; i < 16; ++i) sum += analogReadMilliVolts((uint8_t)kPinBatteryAdc);
  const uint16_t mv = (uint16_t)(sum / 16 * kBatteryDividerRatio);
  static const uint16_t kMv[10] = {4200, 4100, 4000, 3900, 3800, 3700, 3600, 3500, 3400, 3300};
  static const uint8_t kPct[10] = {100, 90, 80, 65, 50, 35, 20, 10, 5, 0};
  uint8_t pct = 0;
  if (mv >= kMv[0]) pct = 100;
  else for (int i = 1; i < 10; ++i)
    if (mv >= kMv[i]) { pct = (uint8_t)(kPct[i] + (kPct[i - 1] - kPct[i]) * (mv - kMv[i]) / (kMv[i - 1] - kMv[i])); break; }
  return BatteryState{true, mv, pct};
}

}  // namespace hg
