#pragma once
// Magnetometer demand, filters, spring, target, calibration session, health (SPEC §6.3, §6.4).
#include "../core/compass_math.h"
#include "../core/health.h"
#include "../core/mag_calibrator.h"
#include "../drivers/mag.h"
#include "../ui/view_model.h"
#include "logger.h"

namespace hg {
struct CompassCtx {
  bool demand;            // a screen needs the heading and the display is on
  uint8_t mount;          // Mount setting (0..7)
  int16_t declination;    // degrees, east positive
  uint8_t needle;         // 0 calm, 1 normal, 2 fast
};

class CompassService {
 public:
  CompassService(II2cBus* bus, HealthRegistry& health, Logger& log) : drv_(*bus), health_(health), log_(log) {}
  void probe(MonoUs now);                         // <= 5 ms; leaves the chip in standby
  void applyCalibration(const MagCal& c);         // from NVS: ignored with W-MAG-CALCHIP if the chip differs
  void tick(MonoUs now, const CompassCtx& c);
  bool standbyForSleep();                         // shutdown sequencer
  bool busy() const { return false; }
  // target
  bool markTarget(int16_t* bearing);              // false: no valid heading yet
  void clearTarget() { target_ = false; }
  bool targetSet() const { return target_; }
  int16_t target() const { return targetDeg_; }
  void restoreTarget(int16_t deg) { target_ = deg >= 0 && deg < 360; targetDeg_ = deg; }
  // calibration session (Settings > Calibrate or Compass #)
  void beginCalibration(MonoUs now);
  void cancelCalibration() { calActive_ = false; haveResult_ = false; }
  bool saveCalibration(MagCal* out);              // READY -> result; the app persists it
  void retryCalibration(MonoUs now) { beginCalibration(now); }
  bool calibrating() const { return calActive_; }
  void resetMinMax();
  void rescan(MonoUs now) { probe(now); }
  MonoUs nextDue(MonoUs now) const;
  const MagView& view() const { return view_; }
  CalView calView(MonoUs now) const;
  MagChip chip() const { return drv_.chip(); }
  const MagCal& calibration() const { return cal_; }
  bool takeBack() { const bool b = back_; back_ = false; return b; }
  uint32_t samples() const { return samples_; }
  bool lastSample(MagSample* s) const { if (!haveSample_) return false; *s = last_; return true; }

 private:
  float lsbPerUt() const { return drv_.lsbPerGauss() / 100.0f; }
  void setHealth(MonoUs now);
  MagDriver drv_;
  HealthRegistry& health_;
  Logger& log_;
  MagView view_{};
  MagCal cal_{};
  bool calForOtherChip_ = false;
  VectorEma ema_;
  Spring spring_;
  bool springInit_ = false;
  MonoUs lastSampleAt_{0}, lastSpringAt_{0}, demandEndedAt_{0}, lastRestart_{0}, nextProbe_{0}, ovfWindow_{0}, fieldBadSince_{0};
  bool demanded_ = false, haveSample_ = false, fieldBad_ = false, back_ = false, lost_ = false;
  uint8_t backoff_ = 0;
  uint16_t ovfInWindow_ = 0, lastOvf_ = 0;
  MagSample last_{};
  bool target_ = false;
  int16_t targetDeg_ = -1;
  bool calActive_ = false, haveResult_ = false;
  MagCalibrator calib_;
  MagCalibrator::Result result_{};
  uint32_t samples_ = 0, rateCount_ = 0;
  MonoUs rateAt_{0};
};
}  // namespace hg
