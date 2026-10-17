#pragma once
// IDisplay on the PC: the frame lives in the u8g2 buffer; spans and commands are counted for tests.
#include "app/ports.h"

namespace hg {
struct FakeDisplay : IDisplay {
  explicit FakeDisplay(u8g2_t* g) : g_(g) {}
  bool present = true, power = true, inverted = false;
  Brightness level = Brightness::Mid;
  uint32_t spans = 0, spanBytes = 0, retries = 0, deepOffs = 0;
  bool ok() const override { return present; }
  u8g2_t* u8g2() override { return g_; }
  void sendPageSpan(uint8_t, uint8_t, uint8_t tw) override { ++spans; spanBytes += tw * 8u; }
  void setPower(bool on) override { power = on; }
  void setLevel(Brightness b) override { level = b; }
  void setContrastRaw(uint8_t) override {}
  void setInvert(bool on) override { inverted = on; }
  void deepOff() override { ++deepOffs; power = false; }
  bool retryInit() override { ++retries; return present; }
  uint32_t ioErrors() const override { return 0; }
 private:
  u8g2_t* g_;
};
}  // namespace hg
