#pragma once
// A u8g2 instance on the PC, set up by the same oledSetup() as the device, with no-op byte/GPIO callbacks.
#include "ui/canvas.h"
#include "ui/oled_setup.h"

namespace hg {
struct HostU8g2 {
  u8g2_t g{};
  HostU8g2() { oledSetup(&g, OledController::Ssd1306, 0x3C, false, u8x8_byte_empty, u8x8_dummy_cb); }
  bool px(int x, int y) { return (u8g2_GetBufferPtr(&g)[(y / 8) * 128 + x] >> (y % 8)) & 1; }
  int lit() { int n = 0; for (int i = 0; i < 1024; ++i) n += __builtin_popcount(u8g2_GetBufferPtr(&g)[i]); return n; }
};
}  // namespace hg
