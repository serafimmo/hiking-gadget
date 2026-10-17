#pragma once
// In-memory I2C bus for driver tests: device models at addresses, an exact transaction log, fault injection.
// Mirrors core 3.3.12 semantics (SPEC §4.4 drivers/i2c_bus.h): a zero-length probe to a missing device is Absent;
// any transfer WITH payload to a missing device is Fail (endTransmission() == 4), a short read is Fail.
#include "drivers/i2c_bus.h"

namespace hg {

class FakeI2cDevice {
 public:
  virtual ~FakeI2cDevice() = default;
  virtual bool ackAddress(MonoUs) { return true; }                       // address phase (probe and every transfer)
  virtual I2cStatus onWrite(const uint8_t* d, size_t n, MonoUs now) = 0;
  virtual I2cStatus onRead(uint8_t* d, size_t n, MonoUs now) = 0;
};

// 256 byte-wide registers: write [reg, data...] stores with auto-increment, write [reg] moves the pointer,
// read returns bytes from the pointer with auto-increment.
class FakeRegDevice : public FakeI2cDevice {
 public:
  uint8_t regs[256] = {};
  uint8_t ptr = 0;
  I2cStatus onWrite(const uint8_t* d, size_t n, MonoUs now) override;
  I2cStatus onRead(uint8_t* d, size_t n, MonoUs now) override;
};

struct I2cTxn {
  enum class Kind : uint8_t { Probe, Write, Read, WriteRead } kind;
  uint8_t addr;
  uint8_t w[16];   // first bytes written (wn may be larger)
  uint8_t wn, rn;
  I2cStatus st;
  MonoUs at;
};

class FakeI2c : public II2cBus {
 public:
  static constexpr size_t kLogMax = 1024;
  void attach(uint8_t addr, FakeI2cDevice* dev) { dev_[addr & 0x7F] = dev; }
  void detach(uint8_t addr) { dev_[addr & 0x7F] = nullptr; }
  void setNow(MonoUs t) { now_ = t; }
  MonoUs now() const { return now_; }
  void failNext(uint8_t count, I2cStatus st = I2cStatus::Fail) { failCount_ = count; failSt_ = st; }
  void setStuck(bool on) { stuck_ = on; }               // every transfer returns Stuck until recover()
  void setRecoverWorks(bool on) { recoverWorks_ = on; }

  size_t txnCount() const { return logN_ < kLogMax ? logN_ : kLogMax; }
  size_t txnTotal() const { return logN_; }             // > kLogMax means the log overflowed
  const I2cTxn& txn(size_t i) const { return log_[i]; }
  void clearLog() { logN_ = 0; }
  bool anyWriteStartingWith(uint8_t addr, uint8_t firstByte) const;   // e.g. "no 0x24.. command was ever sent"
  uint32_t recoverCalls() const { return recoverCalls_; }

  bool probe(uint8_t addr) override;
  I2cStatus write(uint8_t addr, const uint8_t* d, size_t n) override;
  I2cStatus read(uint8_t addr, uint8_t* d, size_t n) override;
  I2cStatus writeRead(uint8_t addr, const uint8_t* w, size_t wn, uint8_t* r, size_t rn) override;
  bool recover() override;
  uint8_t scan(uint8_t* found, uint8_t max) override;
  const I2cStats& stats() const override { return stats_; }

 private:
  bool injected(I2cStatus& st);
  I2cStatus finish(I2cTxn::Kind k, uint8_t addr, const uint8_t* w, size_t wn, size_t rn, I2cStatus st);
  FakeI2cDevice* dev_[128] = {};
  I2cTxn log_[kLogMax] = {};
  size_t logN_ = 0;
  MonoUs now_{0};
  uint8_t failCount_ = 0;
  I2cStatus failSt_ = I2cStatus::Fail;
  bool stuck_ = false, recoverWorks_ = true;
  uint32_t recoverCalls_ = 0;
  I2cStats stats_{};
};

}  // namespace hg
