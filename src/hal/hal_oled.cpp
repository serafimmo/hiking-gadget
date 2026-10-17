#include "build_checks.h"
#include "hal_ports.h"

#include "../config/board_pins.h"
#include "../config/build_config.h"
#include "../ui/oled_setup.h"

// U8g2 C API with our own byte callback on Wire1. Never U8G2_*_2ND_HW_I2C: on core 3.3.12 it compiles to a silent
// no-op and the panel stays black [HWF u8g2 U2/U3] (SPEC §6.6).
namespace hg {
namespace {
uint8_t s_lastErr;
uint32_t s_ioErrors;
uint8_t s_consecErr;
bool s_fastInit;   // warm wake: the panel stayed powered, skip the 3 x 100 ms reset-helper delays

uint8_t hg_byte_wire1_cb(u8x8_t* u, uint8_t msg, uint8_t n, void* p) {
  switch (msg) {
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC: return 1;   // Wire1 is begun by the board, never here
    case U8X8_MSG_BYTE_START_TRANSFER: Wire1.beginTransmission(u8x8_GetI2CAddress(u) >> 1); return 1;
    case U8X8_MSG_BYTE_SEND: Wire1.write((const uint8_t*)p, n); return 1;
    case U8X8_MSG_BYTE_END_TRANSFER:
      s_lastErr = Wire1.endTransmission();
      if (s_lastErr) { ++s_ioErrors; if (s_consecErr < 255) ++s_consecErr; } else s_consecErr = 0;
      return 1;
    default: return 0;
  }
}

uint8_t hg_gpio_delay_cb(u8x8_t*, uint8_t msg, uint8_t n, void*) {
  if (msg == U8X8_MSG_DELAY_MILLI && !s_fastInit) delay(n);
  return 1;
}

bool ack(uint8_t a) {
  Wire1.beginTransmission(a);
  return Wire1.endTransmission() == 0;
}
}  // namespace

// Leaves the panel as the PowerManager last asked (off until its first setPower(true)), at level_, with a blank RAM.
bool HalOled::begin(bool fastInit, Brightness level) {
  addr_ = 0;
  level_ = level;
  for (uint8_t a : kOledAddrs) if (ack(a)) { addr_ = a; break; }
  present_ = addr_ != 0;
  if (!present_) {
    // Headless: set the u8g2 structure up WITHOUT talking to the bus, so the frame buffer exists and the UI code
    // (slides, renders on a later retry) never touches a null buffer.
    if (kOledController == OledController::Sh1106) u8g2_Setup_sh1106_i2c_128x64_noname_f(&g_, U8G2_R0, hg_byte_wire1_cb, hg_gpio_delay_cb);
    else u8g2_Setup_ssd1306_i2c_128x64_noname_f(&g_, U8G2_R0, hg_byte_wire1_cb, hg_gpio_delay_cb);
    return false;
  }
  needInit_ = false;
  s_consecErr = 0;   // transfer errors during the init count: a panel that ACKs but fails the init stays not ok()
  s_fastInit = fastInit && kOledFastWake;
  oledSetup(&g_, kOledController, addr_, kOledFlip, hg_byte_wire1_cb, hg_gpio_delay_cb);
  s_fastInit = false;
  // The panel RAM holds noise after power-on, or the last frame after a warm reset: blank it before the panel is
  // switched on, so the first frames (sent in spans over several loop iterations) never show it (~25 ms at 400 kHz).
  u8g2_ClearBuffer(&g_);
  u8g2_SendBuffer(&g_);
  setLevel(level_);
  u8g2_SetPowerSave(&g_, on_ ? 0 : 1);
  return ok();
}

bool HalOled::ok() const { return present_ && s_consecErr < 3; }

void HalOled::cmds(const uint8_t* c, uint8_t n) {
  u8x8_t* u = u8g2_GetU8x8(&g_);
  u8x8_cad_StartTransfer(u);
  for (uint8_t i = 0; i < n; ++i) u8x8_cad_SendCmd(u, c[i]);
  u8x8_cad_EndTransfer(u);
}

void HalOled::sendPageSpan(uint8_t page, uint8_t tx0, uint8_t tw) {
  if (present_) u8g2_UpdateDisplayArea(&g_, tx0, page, tw, 1);
}

void HalOled::setPower(bool on) {
  on_ = on;
  if (!present_) return;
  if (on && needInit_) { begin(true, level_); return; }   // after deepOff() the charge pump is off: re-init
  u8g2_SetPowerSave(&g_, on ? 0 : 1);
}

// §3.7: contrast, precharge, VCOMH
void HalOled::setLevel(Brightness b) {
  level_ = b;
  if (!present_) return;
  static const uint8_t kTriple[3][3] = {{0x01, 0x22, 0x20}, {0x40, 0xF1, 0x40}, {0xCF, 0xF1, 0x40}};
  const uint8_t* t = kTriple[(int)b % 3];
  const uint8_t c[6] = {0x81, t[0], 0xD9, t[1], 0xDB, t[2]};
  cmds(c, sizeof c);
}

void HalOled::setContrastRaw(uint8_t v) {
  if (!present_) return;
  const uint8_t c[2] = {0x81, v};
  cmds(c, sizeof c);
}

void HalOled::setInvert(bool on) {
  if (!present_) return;
  const uint8_t c[1] = {(uint8_t)(on ? 0xA7 : 0xA6)};
  cmds(c, 1);
}

void HalOled::deepOff() {   // display off + charge pump off: <= 10 uA in deep sleep
  if (!present_) return;
  const uint8_t c[3] = {0xAE, 0x8D, 0x10};
  cmds(c, sizeof c);
  needInit_ = true;   // a rejected deep sleep comes back here: the next power-on re-runs the init
}

// Every 10 s while headless: a zero-length probe first; the full init only when the panel answers (§6.6).
bool HalOled::retryInit() {
  const uint32_t now = millis();
  if (ok()) return true;
  if (now - lastRetryMs_ < 10000 && lastRetryMs_) return false;
  lastRetryMs_ = now;
  uint8_t a = 0;
  for (uint8_t x : kOledAddrs) if (ack(x)) { a = x; break; }
  if (!a) return false;
  return begin(true, level_);   // keeps the level and the on/off state the PowerManager asked for
}

uint32_t HalOled::ioErrors() const { return s_ioErrors; }

}  // namespace hg
