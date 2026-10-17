#include "mag.h"

#include "../config/board_pins.h"
#include "../config/build_config.h"
#include "../config/timing.h"
#include "mag_chips.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }
constexpr int64_t kHmcDiscardUs = 133 * kUsPerMs;   // 2 / 15 Hz
}  // namespace

MagDriver::MagDriver(II2cBus& bus) : bus_(bus) {}

// Identified by the ID register, not by the address (§6.3). A QMC5883L ID mismatch is only a warning.
MagChip MagDriver::probe() {
  chip_ = MagChip::None;
  addr_ = id_ = 0;
  idWarn_ = false;
  bool ackedUnknown = false;
  uint8_t unkAddr = 0, unkId = 0;
  const uint8_t addrs[3] = {kMagAddrQmcL, kMagAddrQmcP, kMagAddrHmc};
  for (uint8_t a : addrs) {
    if (!bus_.probe(a)) continue;
    uint8_t b[3] = {};
    if (a == kMagAddrQmcL) {
      const bool ok = bus_.readRegs(a, qmcl::kChipId, b, 1) == I2cStatus::Ok;
      chip_ = MagChip::Qmc5883l; addr_ = a; id_ = ok ? b[0] : 0;
      idWarn_ = !ok || b[0] != qmcl::kIdExpected;
      return chip_;
    }
    if (a == kMagAddrQmcP) {
      if (bus_.readRegs(a, qmcp::kChipIdReg, b, 1) == I2cStatus::Ok && b[0] == qmcp::kIdExpected) {
        chip_ = MagChip::Qmc5883p; addr_ = a; id_ = b[0];
        return chip_;
      }
    } else if (bus_.readRegs(a, hmc::kId, b, 3) == I2cStatus::Ok && b[0] == 'H' && b[1] == '4' && b[2] == '3') {
      chip_ = MagChip::Hmc5883l; addr_ = a; id_ = b[0];
      return chip_;
    }
    if (!ackedUnknown) { ackedUnknown = true; unkAddr = a; unkId = b[0]; }
  }
  if (ackedUnknown) { chip_ = MagChip::Unknown; addr_ = unkAddr; id_ = unkId; }
  return chip_;
}

void MagDriver::start(MonoUs now) {
  active_ = stuck_ = noStatus_ = dsTested_ = initFailed_ = haveLast_ = false;
  sameCount_ = dsReads_ = 0;
  discard_ = 2;                                   // discard the first 2 samples after start()
  discardUntil_ = now;
  I2cStatus st = I2cStatus::Fail;
  switch (chip_) {
    case MagChip::Qmc5883l: st = bus_.writeReg(addr_, qmcl::kCtrl2, qmcl::kSoftReset); break;
    case MagChip::Qmc5883p: st = bus_.writeReg(addr_, qmcp::kCtrl2, qmcp::kSoftReset); break;
    case MagChip::Hmc5883l: st = I2cStatus::Ok; break;
    default: initFailed_ = true; ph_ = Phase::Off; return;
  }
  if (st != I2cStatus::Ok) { ++ioErr_; initFailed_ = true; ph_ = Phase::Off; return; }
  active_ = true;
  ph_ = Phase::ResetWait;
  due_ = now + magChipInfo(chip_).resetWaitUs;
}

bool MagDriver::configure() {
  bool ok = true;
  auto w = [&](uint8_t reg, uint8_t v) { if (ok && bus_.writeReg(addr_, reg, v) != I2cStatus::Ok) ok = false; };
  switch (chip_) {
    case MagChip::Qmc5883l: {
      w(qmcl::kSetReset, qmcl::kSetResetPeriod);
      w(qmcl::kCtrl2, qmcl::kCtrl2Run);
      w(qmcl::kCtrl1, qmcl::kCtrl1Run);
      uint8_t c1 = 0, sr = 0;
      if (ok) ok = bus_.readRegs(addr_, qmcl::kCtrl1, &c1, 1) == I2cStatus::Ok &&
                   bus_.readRegs(addr_, qmcl::kSetReset, &sr, 1) == I2cStatus::Ok &&
                   c1 == qmcl::kCtrl1Run && sr == qmcl::kSetResetPeriod;
      break;
    }
    case MagChip::Qmc5883p:
      w(qmcp::kCtrl2, 0x00);                      // in case SOFT_RST does not self-clear
      if (kQmc5883pWriteSignReg) w(qmcp::kAxisSign, qmcp::kAxisSignValue);
      w(qmcp::kCtrl2, qmcp::kCtrl2Run);
      w(qmcp::kCtrl1, qmcp::kSuspend);            // mode changes pass through suspend [HWF P-STANDBY]
      w(qmcp::kCtrl1, qmcp::kCtrl1Run);
      break;
    case MagChip::Hmc5883l:
      w(hmc::kCra, hmc::kCraRun);
      w(hmc::kCrb, hmc::kCrbRun);
      w(hmc::kMode, hmc::kModeCont);
      break;
    default:
      ok = false;
  }
  return ok;
}

bool MagDriver::standby() {
  I2cStatus st;
  switch (chip_) {
    case MagChip::Qmc5883l: st = bus_.writeReg(addr_, qmcl::kCtrl1, qmcl::kCtrl1Standby); break;
    case MagChip::Qmc5883p: st = bus_.writeReg(addr_, qmcp::kCtrl1, qmcp::kSuspend); break;
    case MagChip::Hmc5883l: st = bus_.writeReg(addr_, hmc::kMode, hmc::kModeIdle); break;
    default: return false;
  }
  active_ = false;
  ph_ = Phase::Off;
  if (st != I2cStatus::Ok) ++ioErr_;
  return st == I2cStatus::Ok;
}

// Status first, then one burst of all 6 data bytes (a partial read locks the QMC5883L data registers [HWF Q17]).
bool MagDriver::readSample(MonoUs, bool checkStatus, int16_t& x, int16_t& y, int16_t& z, bool& ovf, bool& ready) {
  uint8_t st = 0, b[6];
  ready = false;
  ovf = false;
  const uint8_t statusReg = chip_ == MagChip::Qmc5883l ? qmcl::kStatus : chip_ == MagChip::Qmc5883p ? qmcp::kStatus : hmc::kStatus;
  if (checkStatus) {
    if (bus_.readRegs(addr_, statusReg, &st, 1) != I2cStatus::Ok) { ++ioErr_; return false; }
    if (!(st & 0x01)) return true;                // DRDY / RDY bit 0 on all three
  }
  const uint8_t dataReg = chip_ == MagChip::Qmc5883l ? qmcl::kData : chip_ == MagChip::Qmc5883p ? qmcp::kData : hmc::kData;
  if (bus_.readRegs(addr_, dataReg, b, 6) != I2cStatus::Ok) { ++ioErr_; return false; }
  switch (chip_) {
    case MagChip::Qmc5883l: ovf = decodeQmcL(b, st, x, y, z); break;
    case MagChip::Qmc5883p: ovf = decodeQmcP(b, st, x, y, z); break;
    default:                ovf = decodeHmc(b, x, y, z); break;
  }
  ready = true;
  return true;
}

bool MagDriver::accept(MonoUs now, int16_t x, int16_t y, int16_t z, bool ovf, MagSample* out) {
  lastSample_ = now;
  if (now.v < discardUntil_.v || discard_) {      // HMC: 2/f after start; all: the first 2 samples
    if (discard_) --discard_;
    return false;
  }
  if (haveLast_ && x == last_[0] && y == last_[1] && z == last_[2]) {
    if (++sameCount_ >= kMagStuckSamples) stuck_ = true;
  } else {
    sameCount_ = 0;
  }
  last_[0] = x; last_[1] = y; last_[2] = z;
  haveLast_ = true;
  if (ovf) ++ovf_;
  if (out) *out = MagSample{x, y, z, ovf, now};
  return true;
}

bool MagDriver::poll(MonoUs now, MagSample* out) {
  if (!active_ || stuck_) return false;
  int16_t x = 0, y = 0, z = 0;
  bool ovf = false, ready = false;
  switch (ph_) {
    case Phase::ResetWait:
      if (now.v < due_.v) return false;
      if (!configure()) { ++ioErr_; initFailed_ = true; active_ = false; ph_ = Phase::Off; return false; }
      ph_ = Phase::Running;
      lastSample_ = nextPoll_ = now;
      if (chip_ == MagChip::Hmc5883l) { discardUntil_ = now + kHmcDiscardUs; lastSample_ = discardUntil_; }
      return false;
    case Phase::DeadStatusTest:
      if (now.v < due_.v) return false;
      if (!readSample(now, false, x, y, z, ovf, ready)) return false;
      if (dsReads_ == 0) { dsFirst_[0] = x; dsFirst_[1] = y; dsFirst_[2] = z; }
      else if (x != dsFirst_[0] || y != dsFirst_[1] || z != dsFirst_[2]) dsChanged_ = true;
      due_ = now + magChipInfo(chip_).odrUs;
      if (++dsReads_ < 3) return false;
      ph_ = Phase::Running;
      if (dsChanged_) { noStatus_ = true; lastSample_ = nextPoll_ = now; }   // W-MAG-NOSTATUS: timed reads (D34)
      else stuck_ = true;                                                    // E-MAG-STUCK
      return false;
    case Phase::Running:
      break;
    default:
      return false;
  }
  if (now.v < nextPoll_.v) return false;
  nextPoll_ = now + (noStatus_ ? (int64_t)magChipInfo(chip_).odrUs : ms(kMagPollMs));
  if (!readSample(now, !noStatus_, x, y, z, ovf, ready)) return false;
  if (ready) return accept(now, x, y, z, ovf, out);
  // No new sample: 500 ms without one is stuck, except a 0x1E chip whose RDY may be dead ("DA 5883").
  if (!noStatus_ && now - lastSample_ >= ms(kMagNoStatusMs)) {
    if (addr_ == kMagAddrHmc && !dsTested_) {
      dsTested_ = true;
      dsChanged_ = false;
      dsReads_ = 0;
      ph_ = Phase::DeadStatusTest;
      due_ = now;
    } else if (now - lastSample_ >= ms(kMagStuckMs)) {
      stuck_ = true;
    }
  }
  return false;
}

MagChip MagDriver::chip() const { return chip_; }
uint8_t MagDriver::addr() const { return addr_; }
uint8_t MagDriver::idByte() const { return id_; }
bool MagDriver::idWarning() const { return idWarn_; }
float MagDriver::lsbPerGauss() const { return magChipInfo(chip_).lsbPerGauss; }
DrdyKind MagDriver::drdy() const { return magChipInfo(chip_).drdy; }
bool MagDriver::active() const { return active_; }
bool MagDriver::stuck() const { return stuck_; }
uint16_t MagDriver::ioErrors() const { return ioErr_; }
uint16_t MagDriver::overflows() const { return ovf_; }
bool MagDriver::noStatus() const { return noStatus_; }
bool MagDriver::initFailed() const { return initFailed_; }

}  // namespace hg
