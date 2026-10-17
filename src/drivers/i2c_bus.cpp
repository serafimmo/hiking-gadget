#include "i2c_bus.h"

namespace hg {

I2cStatus II2cBus::writeReg(uint8_t a, uint8_t reg, uint8_t v) {
  const uint8_t b[2] = {reg, v};
  return write(a, b, sizeof b);
}

I2cStatus II2cBus::writeCmd(uint8_t a, uint8_t cmd) {
  return write(a, &cmd, 1);
}

// Sensirion 16-bit commands go out MSB first.
I2cStatus II2cBus::writeCmd16(uint8_t a, uint16_t cmd) {
  const uint8_t b[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
  return write(a, b, sizeof b);
}

// Register pointer write, then a repeated-START read of n bytes.
I2cStatus II2cBus::readRegs(uint8_t a, uint8_t reg, uint8_t* d, size_t n) {
  return writeRead(a, &reg, 1, d, n);
}

}  // namespace hg
