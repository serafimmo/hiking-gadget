#pragma once
// Host-compilable so the init order is unit-tested (§6.6).
// Order is fixed: u8g2_Setup_...(R0) -> u8x8_SetI2CAddress(addr7 << 1) -> u8g2_InitDisplay -> u8g2_SetFlipMode(flip).
// (Setup resets i2c_address to 255, which CAD_INIT turns into 0x78, so the address must be set after Setup [V-src].)
#include "../config/build_config.h"
#include "u8g2_inc.h"
namespace hg {
void oledSetup(u8g2_t* g, OledController c, uint8_t addr7, bool flip, u8x8_msg_cb byteCb, u8x8_msg_cb gpioDelayCb);
}  // namespace hg
