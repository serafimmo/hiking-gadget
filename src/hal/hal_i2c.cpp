#include "build_checks.h"
#include "hal_ports.h"

#include "../config/build_config.h"
#include "../config/timing.h"

namespace hg {

I2cStatus HalI2c::count(I2cStatus s) {
  switch (s) {
    case I2cStatus::Ok: ++stats_.ok; stats_.consecutiveFail = 0; break;
    case I2cStatus::Absent: ++stats_.absent; break;
    default: ++stats_.fail; if (stats_.consecutiveFail < 255) ++stats_.consecutiveFail; break;
  }
  // 3 consecutive failures: check the lines; a held line gets the recovery sequence (§6.1). Reading the lines
  // takes the pins from the I2C peripheral, so the bus is begun again either way.
  if (stats_.consecutiveFail >= 3) {
    stats_.consecutiveFail = 0;
    w_.end();
    if (linesHigh()) {
      w_.begin(sda_, scl_, hz_);
      w_.setTimeOut(kI2cTimeoutMs);
    } else if (!recover()) {
      return I2cStatus::Stuck;
    }
  }
  return s;
}

bool HalI2c::linesHigh() {
  pinMode(sda_, INPUT_PULLUP);
  pinMode(scl_, INPUT_PULLUP);
  delayMicroseconds(50);
  const bool ok = digitalRead(sda_) == HIGH && digitalRead(scl_) == HIGH;
  return ok;
}

bool HalI2c::begin() {
  w_.end();
  if (!linesHigh() && !recover()) { stuck_ = true; return false; }
  stuck_ = false;
  w_.begin(sda_, scl_, hz_);
  w_.setTimeOut(kI2cTimeoutMs);
  return true;
}

// 9 SCL pulses while SDA is low, then a STOP; the bus is begun again either way. True = both lines high.
bool HalI2c::pulse() {
  w_.end();
  pinMode(sda_, INPUT_PULLUP);
  pinMode(scl_, OUTPUT_OPEN_DRAIN);
  digitalWrite(scl_, HIGH);
  delayMicroseconds(5);
  for (int i = 0; i < 9 && digitalRead(sda_) == LOW; ++i) {
    digitalWrite(scl_, LOW);
    delayMicroseconds(5);
    digitalWrite(scl_, HIGH);
    delayMicroseconds(5);
  }
  pinMode(sda_, OUTPUT_OPEN_DRAIN);   // STOP: SDA low -> SCL high -> SDA high
  digitalWrite(sda_, LOW);
  delayMicroseconds(5);
  digitalWrite(scl_, HIGH);
  delayMicroseconds(5);
  digitalWrite(sda_, HIGH);
  delayMicroseconds(5);
  const bool ok = linesHigh();
  w_.begin(sda_, scl_, hz_);
  w_.setTimeOut(kI2cTimeoutMs);
  return ok;
}

// At most 3 recoveries per minute (§6.1); each failed one is counted (the app shows the FATAL card per new failure).
bool HalI2c::recover() {
  const uint32_t now = millis();
  if (now - recWindowStart_ > 60000) { recWindowStart_ = now; recInWindow_ = 0; }
  lastCheckMs_ = now;
  if (recInWindow_ >= kI2cMaxRecoveriesPerMin) { ++stats_.recoveryFails; stuck_ = true; return false; }
  ++recInWindow_;
  const bool ok = pulse();
  if (ok) { ++stats_.recoveries; stuck_ = false; }
  else { ++stats_.recoveryFails; stuck_ = true; }
  return ok;
}

// A stuck bus is down until the next successful check, every 30 s (§6.1). A failed re-check is not a new failure:
// the card was shown once, and re-raising it would wake the display every 30 s. A passed one counts as a recovery.
bool HalI2c::down() {
  if (!stuck_) return false;
  const uint32_t now = millis();
  if (now - lastCheckMs_ < kI2cStuckRecheckMs) return true;
  lastCheckMs_ = now;
  w_.end();
  bool ok = linesHigh();
  if (ok) { w_.begin(sda_, scl_, hz_); w_.setTimeOut(kI2cTimeoutMs); }
  else ok = pulse();
  if (!ok) return true;
  stuck_ = false;
  ++stats_.recoveries;
  return false;
}

bool HalI2c::probe(uint8_t addr) {
  if (down()) return false;
  w_.beginTransmission(addr);
  const uint8_t e = w_.endTransmission();
  if (e == 0) { count(I2cStatus::Ok); return true; }
  count(e == 2 ? I2cStatus::Absent : I2cStatus::Fail);   // core 3.3.12: zero-length NACK = 2 [V-src]
  return false;
}

I2cStatus HalI2c::write(uint8_t addr, const uint8_t* d, size_t n) {
  if (down()) return I2cStatus::Stuck;
  w_.beginTransmission(addr);
  if (n) w_.write(d, n);
  return count(w_.endTransmission() == 0 ? I2cStatus::Ok : I2cStatus::Fail);   // a NACK with payload is 4
}

I2cStatus HalI2c::read(uint8_t addr, uint8_t* d, size_t n) {
  if (down()) return I2cStatus::Stuck;
  if (w_.requestFrom(addr, n) != n) return count(I2cStatus::Fail);   // NACKed header or short: Fail
  for (size_t i = 0; i < n; ++i) d[i] = (uint8_t)w_.read();
  return count(I2cStatus::Ok);
}

I2cStatus HalI2c::writeRead(uint8_t addr, const uint8_t* wb, size_t wn, uint8_t* r, size_t rn) {
  if (down()) return I2cStatus::Stuck;
  w_.beginTransmission(addr);
  w_.write(wb, wn);
  if (w_.endTransmission(false) != 0) return count(I2cStatus::Fail);   // repeated START
  if (w_.requestFrom(addr, rn) != rn) return count(I2cStatus::Fail);
  for (size_t i = 0; i < rn; ++i) r[i] = (uint8_t)w_.read();
  return count(I2cStatus::Ok);
}

uint8_t HalI2c::scan(uint8_t* found, uint8_t max) {
  uint8_t n = 0;
  if (down()) return 0;
  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    w_.beginTransmission(a);
    if (w_.endTransmission() == 0 && n < max) found[n++] = a;
  }
  return n;
}

}  // namespace hg
