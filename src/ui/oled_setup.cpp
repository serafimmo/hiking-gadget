#include "oled_setup.h"

namespace hg {

void oledSetup(u8g2_t* g, OledController c, uint8_t addr7, bool flip, u8x8_msg_cb byteCb, u8x8_msg_cb gpioDelayCb) {
  if (c == OledController::Sh1106) u8g2_Setup_sh1106_i2c_128x64_noname_f(g, U8G2_R0, byteCb, gpioDelayCb);
  else u8g2_Setup_ssd1306_i2c_128x64_noname_f(g, U8G2_R0, byteCb, gpioDelayCb);
  u8x8_SetI2CAddress(u8g2_GetU8x8(g), (uint8_t)(addr7 << 1));   // after Setup: Setup resets it to 255 (-> 0x78)
  u8g2_InitDisplay(g);                                            // the panel stays in power save until setPower(true)
  u8g2_SetFlipMode(g, flip ? 1 : 0);
}

}  // namespace hg
