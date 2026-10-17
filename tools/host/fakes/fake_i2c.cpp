#include "fakes/fake_i2c.h"

#include <cstring>

namespace hg {

I2cStatus FakeRegDevice::onWrite(const uint8_t* d, size_t n, MonoUs) {
  if (n == 0) return I2cStatus::Ok;
  ptr = d[0];
  for (size_t i = 1; i < n; ++i) regs[ptr++] = d[i];
  return I2cStatus::Ok;
}

I2cStatus FakeRegDevice::onRead(uint8_t* d, size_t n, MonoUs) {
  for (size_t i = 0; i < n; ++i) d[i] = regs[ptr++];
  return I2cStatus::Ok;
}

bool FakeI2c::injected(I2cStatus& st) {
  if (stuck_) { st = I2cStatus::Stuck; return true; }
  if (failCount_) { --failCount_; st = failSt_; return true; }
  return false;
}

I2cStatus FakeI2c::finish(I2cTxn::Kind k, uint8_t addr, const uint8_t* w, size_t wn, size_t rn, I2cStatus st) {
  if (logN_ < kLogMax) {
    I2cTxn& t = log_[logN_];
    t = I2cTxn{};
    t.kind = k; t.addr = addr; t.wn = (uint8_t)(wn > 255 ? 255 : wn); t.rn = (uint8_t)(rn > 255 ? 255 : rn);
    if (w) memcpy(t.w, w, wn < sizeof t.w ? wn : sizeof t.w);
    t.st = st; t.at = now_;
  }
  ++logN_;
  switch (st) {
    case I2cStatus::Ok:     ++stats_.ok; stats_.consecutiveFail = 0; break;
    case I2cStatus::Absent: ++stats_.absent; break;
    default:                ++stats_.fail; if (stats_.consecutiveFail < 255) ++stats_.consecutiveFail; break;
  }
  return st;
}

bool FakeI2c::probe(uint8_t addr) {
  I2cStatus st;
  if (!injected(st)) {
    FakeI2cDevice* d = dev_[addr & 0x7F];
    st = d && d->ackAddress(now_) ? I2cStatus::Ok : I2cStatus::Absent;
  }
  return finish(I2cTxn::Kind::Probe, addr, nullptr, 0, 0, st) == I2cStatus::Ok;
}

I2cStatus FakeI2c::write(uint8_t addr, const uint8_t* d, size_t n) {
  I2cStatus st;
  if (!injected(st)) {
    FakeI2cDevice* dev = dev_[addr & 0x7F];
    st = dev && dev->ackAddress(now_) ? dev->onWrite(d, n, now_) : I2cStatus::Fail;
  }
  return finish(I2cTxn::Kind::Write, addr, d, n, 0, st);
}

I2cStatus FakeI2c::read(uint8_t addr, uint8_t* d, size_t n) {
  I2cStatus st;
  if (!injected(st)) {
    FakeI2cDevice* dev = dev_[addr & 0x7F];
    st = dev && dev->ackAddress(now_) ? dev->onRead(d, n, now_) : I2cStatus::Fail;
  }
  return finish(I2cTxn::Kind::Read, addr, nullptr, 0, n, st);
}

I2cStatus FakeI2c::writeRead(uint8_t addr, const uint8_t* w, size_t wn, uint8_t* r, size_t rn) {
  I2cStatus st;
  if (!injected(st)) {
    FakeI2cDevice* dev = dev_[addr & 0x7F];
    if (!dev || !dev->ackAddress(now_)) st = I2cStatus::Fail;
    else {
      st = dev->onWrite(w, wn, now_);
      if (st == I2cStatus::Ok) st = dev->onRead(r, rn, now_);
    }
  }
  return finish(I2cTxn::Kind::WriteRead, addr, w, wn, rn, st);
}

bool FakeI2c::recover() {
  ++recoverCalls_;
  if (recoverWorks_) stuck_ = false;
  if (recoverWorks_) ++stats_.recoveries; else ++stats_.recoveryFails;
  return recoverWorks_;
}

uint8_t FakeI2c::scan(uint8_t* found, uint8_t max) {
  uint8_t n = 0;
  for (uint8_t a = 0x08; a <= 0x77; ++a)
    if (dev_[a] && dev_[a]->ackAddress(now_) && n < max) found[n++] = a;
  return n;
}

bool FakeI2c::anyWriteStartingWith(uint8_t addr, uint8_t firstByte) const {
  for (size_t i = 0; i < txnCount(); ++i) {
    const I2cTxn& t = log_[i];
    if (t.addr == addr && t.wn > 0 && t.w[0] == firstByte &&
        (t.kind == I2cTxn::Kind::Write || t.kind == I2cTxn::Kind::WriteRead))
      return true;
  }
  return false;
}

}  // namespace hg
