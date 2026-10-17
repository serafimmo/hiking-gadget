#include "fakes/fake_sht.h"

#include "core/crc.h"

namespace hg {

void FakeSht::load(uint16_t w0, uint16_t w1, uint8_t words) {
  out_[0] = (uint8_t)(w0 >> 8); out_[1] = (uint8_t)w0; out_[2] = crc8Sensirion(out_, 2);
  out_[3] = (uint8_t)(w1 >> 8); out_[4] = (uint8_t)w1; out_[5] = crc8Sensirion(out_ + 3, 2);
  if (corruptCrc) { out_[2] ^= 0x5A; corruptCrc = false; }
  outN_ = (uint8_t)(words * 3);
  pending_ = true;
}

bool FakeSht::ackAddress(MonoUs now) {
  if (mute) return false;
  // The SHT4x does not clock-stretch: it NACKs while measuring or heating.
  return !(kind == Kind::Sht4x && now.v < busyUntil_.v);
}

I2cStatus FakeSht::onWrite(const uint8_t* d, size_t n, MonoUs now) {
  if (n == 0) return I2cStatus::Ok;
  if (kind == Kind::Sht4x) {
    const uint8_t c = d[0];
    if (c == 0x24) sawHeaterViaSht3xCmd = true;
    switch (c) {
      case 0xFD: load(rawT, rawRh, 2); readyAt_ = busyUntil_ = now + 8300; return I2cStatus::Ok;
      case 0x89: load((uint16_t)(serial >> 16), (uint16_t)serial, 2); readyAt_ = now + 1000; return I2cStatus::Ok;
      case 0x94: pending_ = false; busyUntil_ = now + 1000; return I2cStatus::Ok;
      case 0x39: case 0x2F: case 0x1E: case 0x32: case 0x24: case 0x15: {
        const bool longPulse = c == 0x39 || c == 0x2F || c == 0x1E;
        ++heaterPulses;
        lastHeaterCmd = c;
        load((uint16_t)(rawT + 0x2000), rawRh, 2);
        readyAt_ = busyUntil_ = now + (longPulse ? 1100000 : 110000) + 8300;
        return I2cStatus::Ok;
      }
      default: return I2cStatus::Fail;          // unknown command byte: NACK
    }
  }
  if (n == 1) return I2cStatus::Ok;             // SHT3x: an incomplete 16-bit command is ACKed and ignored
  const uint16_t c = (uint16_t)((d[0] << 8) | d[1]);
  switch (c) {
    case 0x2400: load(rawT, rawRh, 2); readyAt_ = now + 12500; return I2cStatus::Ok;
    case 0xF32D: load(0x8010, 0, 1); readyAt_ = now; return I2cStatus::Ok;
    case 0x3041: case 0x30A2: return I2cStatus::Ok;
    default: return I2cStatus::Fail;
  }
}

I2cStatus FakeSht::onRead(uint8_t* d, size_t n, MonoUs now) {
  if (!pending_ || now.v < readyAt_.v) return I2cStatus::Fail;   // read header NACK
  if (n > outN_) n = outN_;
  for (size_t i = 0; i < n; ++i) d[i] = out_[i];
  pending_ = false;                                               // data can be read once
  return I2cStatus::Ok;
}

}  // namespace hg
