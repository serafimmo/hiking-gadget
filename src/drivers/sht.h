#pragma once
#include "i2c_bus.h"
namespace hg {
struct ShtSample { float tC, rh; bool hot; };            // raw conversion, no user offset
// From RtcState (§6.2). Declared at namespace scope: a nested struct with default member initializers cannot be a
// default argument of its enclosing class's member (CWG 1397); ShtDriver::ProbeHint names the same type.
struct ShtProbeHint { ShtChip cached = ShtChip::None; uint8_t addr = 0; uint32_t busyWaitUs = 0; };
class ShtDriver {
 public:
  explicit ShtDriver(II2cBus& bus);
  using ProbeHint = ShtProbeHint;
  void beginProbe(MonoUs now, const ProbeHint& hint = {});  // non-blocking sequence §6.2
  enum class Poll : uint8_t { Idle, Busy, Sample, Error };
  Poll tick(MonoUs now, ShtSample* out);                  // call every loop; returns Sample once per finished op
  bool startMeasure(MonoUs now);                          // false if busy/not ready
  bool startHeater(MonoUs now, uint8_t heaterCmd);        // SHT4x only; result is flagged hot
  bool probing() const;  bool busy() const;
  ShtChip chip() const;  uint8_t addr() const;  uint8_t ackAddr() const;  uint32_t serial() const;
  uint16_t crcErrors() const;  uint16_t ioErrors() const;  uint8_t consecutiveErrors() const;

 private:
  enum class Phase : uint8_t { Idle, BusyWait, CachedRead, ProbeAddr, ProbeResetWait, ProbeSerialWait, Sht3xGap,
                               Sht3xStatusWait, MeasureWait, HeaterWait, ErrorPending };
  void startProbeSteps(MonoUs now);
  void finishProbe(ShtChip c, uint8_t addr);
  Poll measureStep(MonoUs now, ShtSample* out, uint32_t retryUs);
  void fail(bool crc);
  II2cBus& bus_;
  Phase ph_ = Phase::Idle;
  ProbeHint hint_{};
  MonoUs due_{0}, deadline_{0};
  uint8_t addrIdx_ = 0;
  bool probing_ = false, hot_ = false, retried_ = false;
  ShtChip chip_ = ShtChip::None;
  uint8_t addr_ = 0, ackAddr_ = 0;
  uint32_t serial_ = 0;
  uint16_t crcErr_ = 0, ioErr_ = 0;
  uint8_t consec_ = 0;
};
}  // namespace hg
