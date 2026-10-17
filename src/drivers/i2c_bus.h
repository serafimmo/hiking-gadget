#pragma once
// I2C bus port used by every driver. The register helpers are real code in i2c_bus.cpp (drivers test against them).
#include "../core/types.h"
namespace hg {
enum class I2cStatus : uint8_t { Ok, Absent, Fail, Stuck };
// Absent: zero-length probe NACK (endTransmission() == 2). Fail: endTransmission 4/5/other, or requestFrom() short.
// Core 3.3.12 maps any NACK *with payload* to 4 and never returns 3 [V-src Wire.cpp; HWF sht4x F21] -> no NackData state.
class II2cBus {                                   // I2cStats: core/types.h
 public:
  virtual ~II2cBus() = default;
  virtual bool      probe(uint8_t addr) = 0;                                        // zero-length write
  virtual I2cStatus write(uint8_t addr, const uint8_t* d, size_t n) = 0;            // with STOP
  virtual I2cStatus read(uint8_t addr, uint8_t* d, size_t n) = 0;                   // requestFrom; Fail if short
  virtual I2cStatus writeRead(uint8_t addr, const uint8_t* w, size_t wn, uint8_t* r, size_t rn) = 0;  // repeated START
  virtual bool      recover() = 0;                                                  // 9 SCL + STOP + re-begin
  virtual uint8_t   scan(uint8_t* found, uint8_t max) = 0;                          // 0x08..0x77
  virtual const I2cStats& stats() const = 0;
  I2cStatus writeReg(uint8_t a, uint8_t reg, uint8_t v);                            // helpers in i2c_bus.cpp
  I2cStatus writeCmd(uint8_t a, uint8_t cmd);
  I2cStatus writeCmd16(uint8_t a, uint16_t cmd);
  I2cStatus readRegs(uint8_t a, uint8_t reg, uint8_t* d, size_t n);
};
}  // namespace hg
