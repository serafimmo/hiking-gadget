#pragma once
#include "i2c_bus.h"
namespace hg {
struct MagSample { int16_t x, y, z; bool overflow; MonoUs at; };    // BOARD frame (HMC/QMC5883L axis convention)
class MagDriver {
 public:
  explicit MagDriver(II2cBus& bus);
  MagChip probe();                       // <= 5 ms: 0x0D, 0x2C, 0x1E (zero-length probe, then ID registers)
  void start(MonoUs now);                // soft reset now; configuration completes inside poll() after the reset wait
  bool standby();                        // §6.3 per chip
  bool poll(MonoUs now, MagSample* out); // status read, then burst read if ready; true on a new valid sample
  MagChip chip() const;  uint8_t addr() const;  uint8_t idByte() const;  bool idWarning() const;
  float lsbPerGauss() const;  DrdyKind drdy() const;  bool active() const;
  bool stuck() const;  uint16_t ioErrors() const;  uint16_t overflows() const;
  bool noStatus() const;                 // 0x1E chip with a dead RDY bit: timed reads every 1/ODR (W-MAG-NOSTATUS, §6.3)

  bool initFailed() const;               // the last start() could not configure the chip (I/O or read-back mismatch)

 private:
  enum class Phase : uint8_t { Off, ResetWait, Running, DeadStatusTest };
  bool configure();
  bool readSample(MonoUs now, bool checkStatus, int16_t& x, int16_t& y, int16_t& z, bool& ovf, bool& ready);
  bool accept(MonoUs now, int16_t x, int16_t y, int16_t z, bool ovf, MagSample* out);
  II2cBus& bus_;
  MagChip chip_ = MagChip::None;
  uint8_t addr_ = 0, id_ = 0;
  bool idWarn_ = false;
  Phase ph_ = Phase::Off;
  MonoUs due_{0}, lastSample_{0}, nextPoll_{0}, discardUntil_{0};
  uint8_t discard_ = 0, sameCount_ = 0, dsReads_ = 0;
  bool active_ = false, stuck_ = false, noStatus_ = false, dsTested_ = false, dsChanged_ = false, initFailed_ = false;
  bool haveLast_ = false;
  int16_t last_[3] = {}, dsFirst_[3] = {};
  uint16_t ioErr_ = 0, ovf_ = 0;
};
}  // namespace hg
