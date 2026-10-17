#include "compass_service.h"

#include <math.h>

#include "../config/timing.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }
}  // namespace

void CompassService::setHealth(MonoUs now) {
  ErrCode e = ErrCode::None;
  Health h = Health::Ok;
  switch (drv_.chip()) {
    case MagChip::None: e = ErrCode::MagNone; h = Health::Absent; break;
    case MagChip::Unknown: e = ErrCode::MagUnknown; h = Health::Fault; break;
    default:
      if (drv_.stuck()) { e = ErrCode::MagStuck; h = Health::Fault; }
      else if (view_.saturated) { e = ErrCode::MagSat; h = Health::Degraded; }
      else if (drv_.noStatus()) { e = ErrCode::MagNoStatus; h = Health::Degraded; }
      else if (drv_.idWarning()) { e = ErrCode::MagIdWarn; h = Health::Degraded; }
      else if (calForOtherChip_) { e = ErrCode::MagCalOtherChip; h = Health::Degraded; }
      else if (!cal_.valid) { e = ErrCode::MagUncal; h = Health::Ok; }
      break;
  }
  view_.h = h;
  view_.err = e;
  health_.set(Device::Mag, h, e, now);
}

void CompassService::probe(MonoUs now) {
  const MagChip c = drv_.probe();
  view_.chip = c;
  view_.addr = drv_.addr();
  view_.idByte = drv_.idByte();
  view_.idWarning = drv_.idWarning();
  view_.drdy = drv_.drdy();
  if (c == MagChip::None) {
    log_.log(LogLevel::E, "MAG", "E-MAG-NONE nothing at 0x0D/0x2C/0x1E");
    lost_ = true;
  } else if (c == MagChip::Unknown) {
    log_.log(LogLevel::E, "MAG", "E-MAG-UNK ACK at 0x%02X, ID 0x%02X", drv_.addr(), drv_.idByte());
    lost_ = true;
  } else {
    log_.log(LogLevel::I, "MAG", "%s @0x%02X id=%02X%s", c == MagChip::Qmc5883l ? "QMC5883L" : c == MagChip::Qmc5883p ? "QMC5883P" : "HMC5883L",
             drv_.addr(), drv_.idByte(), drv_.idWarning() ? " (W-MAG-ID: assumed QMC5883L)" : "");
    if (lost_) back_ = true;
    lost_ = false;
    backoff_ = 0;
    drv_.standby();
    if (cal_.valid || calForOtherChip_) applyCalibration(cal_);
  }
  if (c == MagChip::None || c == MagChip::Unknown) {
    nextProbe_ = now + ms(kReprobeBackoffMs[backoff_ < 3 ? backoff_ : 3]);
    if (backoff_ < 3) ++backoff_;
  }
  setHealth(now);
}

void CompassService::applyCalibration(const MagCal& c) {
  cal_ = c;
  calForOtherChip_ = false;
  if (c.valid && drv_.chip() != MagChip::None && drv_.chip() != MagChip::Unknown && c.chip != (uint8_t)drv_.chip()) {
    calForOtherChip_ = true;
    cal_.valid = false;
    log_.log(LogLevel::W, "MAG", "W-MAG-CALCHIP calibration was made for another chip, ignored");
  }
  view_.calibrated = cal_.valid;
}

bool CompassService::markTarget(int16_t* bearing) {
  if (!view_.valid) return false;
  targetDeg_ = (int16_t)(lroundf(wrap360(view_.headingDeg)) % 360);
  target_ = true;
  if (bearing) *bearing = targetDeg_;
  return true;
}

void CompassService::beginCalibration(MonoUs now) {
  calib_.begin((uint8_t)drv_.chip(), lsbPerUt(), now);
  calActive_ = true;
  haveResult_ = false;
}

bool CompassService::saveCalibration(MagCal* out) {
  result_ = calib_.result();
  haveResult_ = true;
  calActive_ = false;
  if (result_.grade == MagCalibrator::Grade::Poor) return false;
  cal_ = result_.cal;
  cal_.chip = (uint8_t)drv_.chip();
  cal_.valid = true;
  calForOtherChip_ = false;
  view_.calibrated = true;
  if (out) *out = cal_;
  return true;
}

CalView CompassService::calView(MonoUs now) const {
  CalView v{};
  v.p = calib_.progress(now);
  v.haveResult = haveResult_;
  v.r = result_;
  return v;
}

void CompassService::resetMinMax() {
  for (int i = 0; i < 3; ++i) { view_.mn[i] = view_.mx[i] = view_.raw[i]; }
}

bool CompassService::standbyForSleep() { return drv_.chip() == MagChip::None || drv_.chip() == MagChip::Unknown || drv_.standby(); }

MonoUs CompassService::nextDue(MonoUs now) const {
  if (demanded_) return now + ms(kMagPollMs);
  if (drv_.active()) return demandEndedAt_ + ms(kMagStandbyGraceMs);
  return MonoUs{INT64_MAX};
}

void CompassService::tick(MonoUs now, const CompassCtx& c) {
  const bool present = drv_.chip() != MagChip::None && drv_.chip() != MagChip::Unknown;
  if (!present) {
    view_.valid = false;
    if (c.demand && now.v >= nextProbe_.v) probe(now);
    return;
  }
  const bool want = c.demand || calActive_;
  if (want && !demanded_) { demanded_ = true; if (!drv_.active()) { drv_.start(now); lastSampleAt_ = now; } }
  if (!want && demanded_) { demanded_ = false; demandEndedAt_ = now; }
  if (!demanded_) {
    if (drv_.active() && now - demandEndedAt_ >= ms(kMagStandbyGraceMs)) drv_.standby();
    return;
  }
  if (!drv_.active()) {
    if (drv_.initFailed() && now - lastRestart_ >= 5 * kUsPerS) { lastRestart_ = now; drv_.start(now); }
    return;
  }
  if (drv_.stuck()) {
    setHealth(now);
    if (now - lastRestart_ >= 5 * kUsPerS) {
      log_.log(LogLevel::E, "MAG", "E-MAG-STUCK no new data, restarting");
      lastRestart_ = now;
      drv_.start(now);
    }
    return;
  }
  MagSample s;
  if (!drv_.poll(now, &s)) return;
  ++samples_;
  last_ = s;
  haveSample_ = true;
  view_.raw[0] = s.x; view_.raw[1] = s.y; view_.raw[2] = s.z;
  for (int i = 0; i < 3; ++i) {
    const int16_t v = view_.raw[i];
    if (!view_.valid || v < view_.mn[i]) view_.mn[i] = v;
    if (!view_.valid || v > view_.mx[i]) view_.mx[i] = v;
  }
  // overflow rate > 10/s -> SATURATED
  if (now - ovfWindow_ >= kUsPerS) { view_.saturated = ovfInWindow_ > 10; ovfWindow_ = now; ovfInWindow_ = 0; }
  if (s.overflow) ++ovfInWindow_;
  ++rateCount_;
  if (now - rateAt_ >= kUsPerS) { view_.rateHz = (uint16_t)rateCount_; rateCount_ = 0; rateAt_ = now; }
  if (calActive_) calib_.add(s.x, s.y, s.z, s.overflow, now);
  if (s.overflow) { setHealth(now); return; }
  float F, L;
  toDevice(cal_, mountFromSetting(c.mount), (float)s.x, (float)s.y, F, L);
  const float dt = lastSampleAt_.v ? (float)(now - lastSampleAt_) / 1e6f : 0.02f;
  lastSampleAt_ = now;
  ema_.push(F, L, dt, kNeedleTauS[c.needle % 3]);
  const float mag = headingDeg(ema_.fwd(), ema_.left());
  const float trueH = wrap360(mag + (float)c.declination);
  view_.headingDeg = trueH;
  view_.shownDeg = shownHeading(trueH, view_.valid ? view_.shownDeg : (int16_t)-1);
  view_.trueRef = c.declination != 0;
  view_.fieldUt = hypotf((float)s.x, (float)s.y) / (lsbPerUt() > 0 ? lsbPerUt() : 1.0f);
  if (cal_.valid && cal_.rCalCounts > 0) {
    const float r = hypotf(ema_.fwd(), ema_.left()) / cal_.rCalCounts;   // corrected radius ~ rCal when clean
    const bool bad = r < 0.6f || r > 1.6f;
    if (bad && !fieldBad_) fieldBadSince_ = now;
    fieldBad_ = bad;
    view_.fieldSuspect = bad && now - fieldBadSince_ > kUsPerS;
  } else {
    view_.fieldSuspect = false;
  }
  if (!springInit_) { spring_.snap(trueH); springInit_ = true; lastSpringAt_ = now; }
  view_.valid = true;
  view_.calibrated = cal_.valid;
  // the card follows the heading with the spring (dt clamped inside)
  const float sdt = (float)(now - lastSpringAt_) / 1e6f;
  lastSpringAt_ = now;
  view_.cardDeg = spring_.update(trueH, sdt, kNeedleOmega[c.needle % 3]);
  setHealth(now);
}

}  // namespace hg
