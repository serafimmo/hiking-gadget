#include "fakes/fake_mag.h"

#include <cstring>

namespace hg {

FakeMag::FakeMag(Kind k) : kind(k) { reset(); }

void FakeMag::reset() {
  memset(regs, 0, sizeof regs);
  ptr = 0;
  if (kind == Kind::QmcL) regs[0x0D] = 0xFF;
  if (kind == Kind::QmcP) regs[0x00] = 0x80;
  if (kind == Kind::Hmc) { regs[0x0A] = 'H'; regs[0x0B] = '4'; regs[0x0C] = '3'; regs[0x00] = 0x10; regs[0x01] = 0x20; regs[0x02] = 0x01; }
  odrUs_ = kind == Kind::Hmc ? 66667 : 20000;
}

bool FakeMag::running() const {
  switch (kind) {
    case Kind::QmcL: return (regs[0x09] & 0x03) == 0x01;
    case Kind::QmcP: return (regs[0x0A] & 0x03) != 0x00;
    default:         return (regs[0x02] & 0x03) == 0x00;
  }
}

void FakeMag::store() {
  // dither the LSB so consecutive samples differ, unless frozen
  const int16_t d = frozen ? 0 : (int16_t)(samples & 1);
  const int16_t x = (int16_t)(bx + d), y = by, z = bz;
  auto le = [&](uint8_t r, int16_t v) { regs[r] = (uint8_t)v; regs[r + 1] = (uint8_t)((uint16_t)v >> 8); };
  auto be = [&](uint8_t r, int16_t v) { regs[r] = (uint8_t)((uint16_t)v >> 8); regs[r + 1] = (uint8_t)v; };
  switch (kind) {
    case Kind::QmcL:
      le(0x00, x); le(0x02, y); le(0x04, z);
      regs[0x06] = (uint8_t)(0x01 | (overflow ? 0x02 : 0));
      break;
    case Kind::QmcP:   // board = (py, -px, pz)  =>  px = -by, py = bx
      le(0x01, (int16_t)-y); le(0x03, x); le(0x05, z);
      regs[0x09] = (uint8_t)(0x01 | (overflow ? 0x02 : 0));
      break;
    case Kind::Hmc:
      be(0x03, overflow ? (int16_t)-4096 : x); be(0x05, z); be(0x07, y);
      if (!deadStatus) regs[0x09] |= 0x01;
      break;
  }
}

void FakeMag::advance(MonoUs now) {
  if (!running() || stopped) return;
  if (next_.v == 0) next_ = now + odrUs_;
  while (now.v >= next_.v) {
    ++samples;
    store();
    next_ = next_ + odrUs_;
  }
}

bool FakeMag::ackAddress(MonoUs now) { advance(now); return true; }

I2cStatus FakeMag::onWrite(const uint8_t* d, size_t n, MonoUs now) {
  if (n >= 2) {
    const uint8_t reg = d[0], v = d[1];
    if ((kind == Kind::QmcL && reg == 0x0A && (v & 0x80)) || (kind == Kind::QmcP && reg == 0x0B && (v & 0x80))) {
      reset();
      next_ = MonoUs{0};
      return I2cStatus::Ok;
    }
  }
  const bool wasRunning = running();
  FakeRegDevice::onWrite(d, n, now);
  if (!wasRunning && running()) next_ = now + odrUs_;
  return I2cStatus::Ok;
}

I2cStatus FakeMag::onRead(uint8_t* d, size_t n, MonoUs now) {
  advance(now);
  const uint8_t start = ptr;
  FakeRegDevice::onRead(d, n, now);
  const auto covers = [&](uint8_t r) { return r >= start && r < start + n; };
  if (kind == Kind::QmcL && covers(0x05)) regs[0x06] &= (uint8_t)~0x01;   // data read clears DRDY
  if (kind == Kind::QmcP && covers(0x09)) regs[0x09] &= (uint8_t)~0x01;   // status read clears DRDY
  if (kind == Kind::Hmc && covers(0x08)) regs[0x09] &= (uint8_t)~0x01;
  return I2cStatus::Ok;
}

}  // namespace hg
