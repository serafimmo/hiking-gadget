#pragma once
// SHT scheduling, heater + cool-down, publishing, health (SPEC §4.5 table, §6.2).
#include "../core/health.h"
#include "../core/heater_policy.h"
#include "../drivers/sht.h"
#include "../ui/view_model.h"
#include "logger.h"

namespace hg {
struct ThermoCtx {
  bool displayOn;
  bool fast;              // Home visible: 2 s cadence
  int16_t offsetTenths;   // Temp offset setting
  HeaterMode mode;
  HeaterPower power;
  bool shutdownPending;
};

class ThermoService {
 public:
  ThermoService(II2cBus* bus, HealthRegistry& health, Logger& log) : drv_(*bus), health_(health), log_(log) {}
  void beginProbe(MonoUs now, const ShtProbeHint& hint);
  void tick(MonoUs now, const ThermoCtx& c);
  void measureSoon() { measureNow_ = true; }              // display wake, SHT command
  bool heaterTest(MonoUs now, const ThermoCtx& c, HeaterDecision* why);   // Diagnostics / HEAT (guarded)
  bool probing() const { return drv_.probing(); }
  bool busy() const { return drv_.busy() || drv_.probing(); }
  ShtChip chip() const { return drv_.chip(); }
  uint8_t addr() const { return drv_.addr() ? drv_.addr() : drv_.ackAddr(); }
  MonoUs nextDue(MonoUs now, const ThermoCtx& c) const;
  const ThermoView& view() const { return view_; }
  // one-shot events for the app
  bool takeHeaterAck() { const bool a = heaterAck_; heaterAck_ = false; return a; }
  bool takeBack() { const bool b = back_; back_ = false; return b; }
  bool takeSkipHot() { const bool s = skipHotEvent_; skipHotEvent_ = false; return s; }
  bool takeProbeDone() { const bool p = probeDone_; probeDone_ = false; return p; }
  float lastRawT() const { return tRaw_; }
  float lastRawRh() const { return rhRaw_; }
  uint16_t crcErrors() const { return drv_.crcErrors(); }
  uint16_t ioErrors() const { return drv_.ioErrors(); }
  bool heatValid() const { return haveHeat_; }
  float heatPre() const { return heatPre_; }
  float heatHot() const { return heatHot_; }

 private:
  void onProbeDone(MonoUs now);
  void onSample(MonoUs now, const ShtSample& s, const ThermoCtx& c);
  void onError(MonoUs now);
  void publish(MonoUs now, const ThermoCtx& c);
  void scheduleReprobe(MonoUs now);
  ShtDriver drv_;
  HealthRegistry& health_;
  Logger& log_;
  ThermoView view_{};
  bool wasProbing_ = false, measureNow_ = true, haveValid_ = false, everFound_ = false, lostSinceFound_ = false;
  MonoUs lastMeasure_{0}, lastValid_{0}, nextProbe_{0};
  bool measuredOnce_ = false;
  uint8_t backoff_ = 0;
  float tRaw_ = 0, rhRaw_ = 0;
  // heater
  uint8_t heaterUi_ = 0;                  // 0 off, 1 heating, 2 cooling
  bool havePulse_ = false, haveHeat_ = false;
  MonoUs lastPulse_{0}, coolStart_{0};
  float preT_ = 0, heatPre_ = 0, heatHot_ = 0;
  uint8_t highRhStreak_ = 0;
  HeaterDecision lastSkip_ = HeaterDecision::No;
  float coolT_[6] = {};
  MonoUs coolAt_[6] = {};
  uint8_t coolN_ = 0;
  bool heaterAck_ = false, back_ = false, skipHotEvent_ = false, skipHotShown_ = false, probeDone_ = false;
};
}  // namespace hg
