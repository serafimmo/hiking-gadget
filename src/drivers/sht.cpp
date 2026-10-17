#include "sht.h"

#include "../config/board_pins.h"
#include "../config/timing.h"
#include "../core/crc.h"
#include "../core/env_math.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }
constexpr uint8_t kCmdMeasure = 0xFD, kCmdSerial = 0x89, kCmdReset = 0x94;
constexpr uint16_t kSht3xMeasure = 0x2400, kSht3xStatus = 0xF32D, kSht3xClearStatus = 0x3041;

bool wordOk(const uint8_t* b) { return crc8Sensirion(b, 2) == b[2]; }
uint16_t word(const uint8_t* b) { return (uint16_t)((b[0] << 8) | b[1]); }
}  // namespace

ShtDriver::ShtDriver(II2cBus& bus) : bus_(bus) {}

void ShtDriver::beginProbe(MonoUs now, const ProbeHint& hint) {
  hint_ = hint;
  probing_ = true;
  chip_ = ShtChip::None;
  addr_ = ackAddr_ = 0;
  serial_ = 0;
  retried_ = false;
  if (hint.busyWaitUs > 0) {   // the MCU reset during a heater pulse: let it finish (<= 1.2 s)
    const uint32_t w = hint.busyWaitUs < kShtHeaterBusyMaxMs * 1000u ? hint.busyWaitUs : kShtHeaterBusyMaxMs * 1000u;
    ph_ = Phase::BusyWait;
    due_ = now + w;
    return;
  }
  startProbeSteps(now);
}

// A cached chip (DEEPSLEEP wake) is verified with one harmless command; never 0x2400 here: a sensor swapped
// during sleep could be an SHT4x, for which 0x24 is the 110 mW heater [HWF F18/F19].
void ShtDriver::startProbeSteps(MonoUs now) {
  if (hint_.cached == ShtChip::Sht4x && bus_.writeCmd(hint_.addr, kCmdMeasure) == I2cStatus::Ok) {
    ph_ = Phase::CachedRead;
    due_ = now + ms(kShtMeasWaitMs);
    return;
  }
  if (hint_.cached == ShtChip::Sht3x && bus_.writeCmd16(hint_.addr, kSht3xStatus) == I2cStatus::Ok) {
    ph_ = Phase::CachedRead;
    due_ = now + 2 * kUsPerMs;
    return;
  }
  ph_ = Phase::ProbeAddr;
  addrIdx_ = 0;
  due_ = now;
}

void ShtDriver::finishProbe(ShtChip c, uint8_t addr) {
  chip_ = c;
  addr_ = addr;
  probing_ = false;
  ph_ = Phase::Idle;
  consec_ = 0;
}

void ShtDriver::fail(bool crc) {
  if (crc) ++crcErr_; else ++ioErr_;
  if (consec_ < 255) ++consec_;
}

ShtDriver::Poll ShtDriver::measureStep(MonoUs now, ShtSample* out, uint32_t retryUs) {
  if (now.v < due_.v) return Poll::Busy;
  uint8_t b[6];
  const I2cStatus st = bus_.read(addr_, b, 6);
  if (st != I2cStatus::Ok) {                    // NACK while busy: retry until the deadline
    if (now.v < deadline_.v) {
      due_ = now + retryUs;
      return Poll::Busy;
    }
    fail(false);
    ph_ = Phase::Idle;
    return Poll::Error;
  }
  ph_ = Phase::Idle;
  if (!wordOk(b) || !wordOk(b + 3)) {           // data can be read only once: no retry
    fail(true);
    return Poll::Error;
  }
  consec_ = 0;
  if (out) {
    const bool four = chip_ == ShtChip::Sht4x;
    out->tC = four ? sht4xTempC(word(b)) : sht3xTempC(word(b));
    out->rh = four ? sht4xRh(word(b + 3)) : sht3xRh(word(b + 3));
    out->hot = hot_;
  }
  return Poll::Sample;
}

ShtDriver::Poll ShtDriver::tick(MonoUs now, ShtSample* out) {
  switch (ph_) {
    case Phase::Idle:
      return Poll::Idle;
    case Phase::ErrorPending:
      ph_ = Phase::Idle;
      return Poll::Error;
    case Phase::MeasureWait:
      return measureStep(now, out, 3 * kUsPerMs);
    case Phase::HeaterWait:
      return measureStep(now, out, 5 * kUsPerMs);
    default:
      break;
  }
  if (now.v < due_.v) return Poll::Busy;
  switch (ph_) {
    case Phase::BusyWait:
      startProbeSteps(now);
      return Poll::Busy;
    case Phase::CachedRead: {
      uint8_t b[6];
      if (hint_.cached == ShtChip::Sht4x) {
        if (bus_.read(hint_.addr, b, 6) == I2cStatus::Ok && wordOk(b) && wordOk(b + 3)) {
          finishProbe(ShtChip::Sht4x, hint_.addr);
          ackAddr_ = hint_.addr;
          if (out) *out = ShtSample{sht4xTempC(word(b)), sht4xRh(word(b + 3)), false};
          return Poll::Sample;
        }
      } else if (bus_.read(hint_.addr, b, 3) == I2cStatus::Ok && wordOk(b)) {
        finishProbe(ShtChip::Sht3x, hint_.addr);
        ackAddr_ = hint_.addr;
        return Poll::Idle;
      }
      ph_ = Phase::ProbeAddr;                   // the cached chip did not answer as expected: full probe
      addrIdx_ = 0;
      return Poll::Busy;
    }
    case Phase::ProbeAddr:
      while (addrIdx_ < 2 && !bus_.probe(kShtAddrs[addrIdx_])) ++addrIdx_;
      if (addrIdx_ >= 2) {
        finishProbe(ShtChip::None, 0);
        return Poll::Idle;
      }
      ackAddr_ = kShtAddrs[addrIdx_];
      bus_.writeCmd(ackAddr_, kCmdReset);       // aborts a heater pulse left over from a warm reset, if accepted
      ph_ = Phase::ProbeResetWait;
      due_ = now + 10 * kUsPerMs;
      return Poll::Busy;
    case Phase::ProbeResetWait:
      bus_.writeCmd(ackAddr_, kCmdSerial);
      ph_ = Phase::ProbeSerialWait;
      retried_ = false;
      due_ = now + ms(kShtProbeSerialRetryMs);
      return Poll::Busy;
    case Phase::ProbeSerialWait: {
      uint8_t b[6];
      const I2cStatus st = bus_.read(ackAddr_, b, 6);
      if (st == I2cStatus::Ok && wordOk(b) && wordOk(b + 3)) {
        serial_ = ((uint32_t)word(b) << 16) | word(b + 3);
        finishProbe(ShtChip::Sht4x, ackAddr_);
        return Poll::Idle;
      }
      if (st != I2cStatus::Ok && !retried_) {   // read NACK: one retry
        retried_ = true;
        bus_.writeCmd(ackAddr_, kCmdSerial);
        due_ = now + ms(kShtProbeSerialRetryMs);
        return Poll::Busy;
      }
      ph_ = Phase::Sht3xGap;                    // SHT4x ruled out: only now may an SHT3x command go out
      due_ = now + 40 * kUsPerMs;
      return Poll::Busy;
    }
    case Phase::Sht3xGap:
      if (bus_.writeCmd16(ackAddr_, kSht3xStatus) != I2cStatus::Ok) {
        finishProbe(ShtChip::Unknown, ackAddr_);
        return Poll::Idle;
      }
      ph_ = Phase::Sht3xStatusWait;
      due_ = now + 2 * kUsPerMs;
      return Poll::Busy;
    case Phase::Sht3xStatusWait: {
      uint8_t b[3];
      if (bus_.read(ackAddr_, b, 3) == I2cStatus::Ok && wordOk(b)) {
        bus_.writeCmd16(ackAddr_, kSht3xClearStatus);
        finishProbe(ShtChip::Sht3x, ackAddr_);
      } else {
        finishProbe(ShtChip::Unknown, ackAddr_);
      }
      return Poll::Idle;
    }
    default:
      return Poll::Idle;
  }
}

bool ShtDriver::startMeasure(MonoUs now) {
  if (probing_ || ph_ != Phase::Idle || (chip_ != ShtChip::Sht4x && chip_ != ShtChip::Sht3x)) return false;
  const bool four = chip_ == ShtChip::Sht4x;
  const I2cStatus st = four ? bus_.writeCmd(addr_, kCmdMeasure) : bus_.writeCmd16(addr_, kSht3xMeasure);
  hot_ = false;
  if (st != I2cStatus::Ok) {
    fail(false);
    ph_ = Phase::ErrorPending;
    return true;
  }
  ph_ = Phase::MeasureWait;
  due_ = now + ms(four ? kShtMeasWaitMs : kSht3xMeasWaitMs);
  deadline_ = now + ms(kShtReadDeadlineMs);
  return true;
}

bool ShtDriver::startHeater(MonoUs now, uint8_t heaterCmd) {
  if (probing_ || ph_ != Phase::Idle || chip_ != ShtChip::Sht4x) return false;
  hot_ = true;
  if (bus_.writeCmd(addr_, heaterCmd) != I2cStatus::Ok) {
    fail(false);
    ph_ = Phase::ErrorPending;
    return true;
  }
  ph_ = Phase::HeaterWait;
  due_ = now + ms(kShtHeaterWaitMs);
  deadline_ = now + ms(kShtHeaterWaitMs + kShtHeaterDeadlineMs);
  return true;
}

bool ShtDriver::probing() const { return probing_; }
bool ShtDriver::busy() const { return ph_ == Phase::MeasureWait || ph_ == Phase::HeaterWait || ph_ == Phase::ErrorPending; }
ShtChip ShtDriver::chip() const { return chip_; }
uint8_t ShtDriver::addr() const { return addr_; }
uint8_t ShtDriver::ackAddr() const { return ackAddr_; }
uint32_t ShtDriver::serial() const { return serial_; }
uint16_t ShtDriver::crcErrors() const { return crcErr_; }
uint16_t ShtDriver::ioErrors() const { return ioErr_; }
uint8_t ShtDriver::consecutiveErrors() const { return consec_; }

}  // namespace hg
