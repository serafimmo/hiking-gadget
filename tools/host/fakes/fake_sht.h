#pragma once
// SHT4x / SHT3x device models for FakeI2c (SPEC §6.2 command sets and timing).
#include "fakes/fake_i2c.h"

namespace hg {

class FakeSht : public FakeI2cDevice {
 public:
  enum class Kind : uint8_t { Sht4x, Sht3x };
  explicit FakeSht(Kind k) : kind(k) {}
  Kind kind;
  uint16_t rawT = 0x6666, rawRh = 0x8000;
  uint32_t serial = 0x0F3A21C4;
  bool corruptCrc = false;      // next data words carry a bad CRC
  bool mute = false;            // answers nothing (NACKs everything)
  bool sawHeaterViaSht3xCmd = false;   // SHT4x received a 0x24.. command (the 110 mW heater)
  int heaterPulses = 0;
  uint8_t lastHeaterCmd = 0;

  bool ackAddress(MonoUs now) override;
  I2cStatus onWrite(const uint8_t* d, size_t n, MonoUs now) override;
  I2cStatus onRead(uint8_t* d, size_t n, MonoUs now) override;

 private:
  void load(uint16_t w0, uint16_t w1, uint8_t words);
  uint8_t out_[6] = {};
  uint8_t outN_ = 0;
  MonoUs readyAt_{0}, busyUntil_{0};
  bool pending_ = false;
};

}  // namespace hg
