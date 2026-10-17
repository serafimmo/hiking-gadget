// ui/oled_setup with a recording byte callback (SPEC §6.6): the address is set after Setup, flip after init.
#include <cstring>
#include <initializer_list>

#include "tiny_test.h"
#include "ui/oled_setup.h"

using namespace hg;

namespace {
struct Rec {
  uint8_t addr[512];
  uint8_t bytes[2048];
  int nT = 0, nB = 0;
  bool flipSeen = false;
} g;
uint8_t recCb(u8x8_t* u, uint8_t msg, uint8_t n, void* p) {
  switch (msg) {
    case U8X8_MSG_BYTE_START_TRANSFER: if (g.nT < 512) g.addr[g.nT++] = (uint8_t)(u8x8_GetI2CAddress(u) >> 1); return 1;
    case U8X8_MSG_BYTE_SEND:
      for (uint8_t i = 0; i < n && g.nB < 2048; ++i) g.bytes[g.nB++] = ((const uint8_t*)p)[i];
      return 1;
    default: return 1;
  }
}
}  // namespace

TEST(oled_setup_address_after_setup) {
  for (uint8_t a7 : {(uint8_t)0x3D, (uint8_t)0x3C}) {
    g = Rec{};
    u8g2_t u{};
    oledSetup(&u, OledController::Ssd1306, a7, false, recCb, u8x8_dummy_cb);
    CHECK(g.nT > 3);
    int wrong = 0;
    for (int i = 0; i < g.nT; ++i) wrong += g.addr[i] != a7;
    CHECK_EQ(wrong, 0);                                   // every transfer goes to the probed address
  }
}

TEST(oled_setup_flip_after_init) {
  // The init sequence sends the default remap (0xA1, 0xC8); SetFlipMode, called after it, decides the last word.
  auto lastIdx = [](uint8_t b) { int k = -1; for (int i = 0; i < g.nB; ++i) if (g.bytes[i] == b) k = i; return k; };
  g = Rec{};
  u8g2_t u{};
  oledSetup(&u, OledController::Ssd1306, 0x3C, true, recCb, u8x8_dummy_cb);
  CHECK(lastIdx(0xA0) > lastIdx(0xA1));
  CHECK(lastIdx(0xC0) > lastIdx(0xC8));
  g = Rec{};
  oledSetup(&u, OledController::Ssd1306, 0x3C, false, recCb, u8x8_dummy_cb);
  CHECK(lastIdx(0xA1) > lastIdx(0xA0));
  CHECK(lastIdx(0xC8) > lastIdx(0xC0));
}
