#pragma once
// QMC5883L / QMC5883P / HMC5883L register models for FakeI2c (SPEC §6.3). The field is set in the BOARD frame;
// each model stores it in its own layout (LE / rotated LE / BE X,Z,Y), so the driver's decoders are really tested.
#include "fakes/fake_i2c.h"

namespace hg {

class FakeMag : public FakeRegDevice {
 public:
  enum class Kind : uint8_t { QmcL, QmcP, Hmc };
  explicit FakeMag(Kind k);
  Kind kind;
  int16_t bx = 1000, by = -500, bz = 3000;   // board-frame field
  bool frozen = false;       // data never changes (identical samples)
  bool deadStatus = false;   // HMC map: RDY never sets while data keep changing ("DA 5883")
  bool stopped = false;      // no new samples at all
  bool overflow = false;     // next samples flagged overflow
  uint32_t samples = 0;

  bool ackAddress(MonoUs now) override;
  I2cStatus onWrite(const uint8_t* d, size_t n, MonoUs now) override;
  I2cStatus onRead(uint8_t* d, size_t n, MonoUs now) override;

 private:
  void reset();
  bool running() const;
  void advance(MonoUs now);
  void store();
  MonoUs next_{0};
  uint32_t odrUs_ = 20000;
};

}  // namespace hg
