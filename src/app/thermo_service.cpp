#include "thermo_service.h"

#include <math.h>

#include "../config/timing.h"
#include "../core/env_math.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }
}  // namespace

void ThermoService::beginProbe(MonoUs now, const ShtProbeHint& hint) {
  drv_.beginProbe(now, hint);
  wasProbing_ = true;
}

void ThermoService::scheduleReprobe(MonoUs now) {
  nextProbe_ = now + ms(kReprobeBackoffMs[backoff_ < 3 ? backoff_ : 3]);
  if (backoff_ < 3) ++backoff_;
}

void ThermoService::onProbeDone(MonoUs now) {
  probeDone_ = true;
  view_.chip = drv_.chip();
  view_.addr = addr();
  view_.serial = drv_.serial();
  switch (drv_.chip()) {
    case ShtChip::Sht4x:
    case ShtChip::Sht3x:
      health_.set(Device::Sht, Health::Ok, drv_.chip() == ShtChip::Sht3x ? ErrCode::Sht3xInfo : ErrCode::None, now);
      log_.log(LogLevel::I, "SHT", "%s @0x%02X sn=%08lX", drv_.chip() == ShtChip::Sht4x ? "SHT4x" : "SHT3x", drv_.addr(),
               (unsigned long)drv_.serial());
      if (lostSinceFound_) back_ = true;
      everFound_ = true;
      lostSinceFound_ = false;
      backoff_ = 0;
      measureNow_ = true;
      view_.err = ErrCode::None;
      view_.h = Health::Ok;
      break;
    case ShtChip::Unknown:
      health_.set(Device::Sht, Health::Fault, ErrCode::ShtUnknown, now);
      log_.log(LogLevel::E, "SHT", "E-SHT-UNK ACK at 0x%02X but not SHT4x/SHT3x", drv_.ackAddr());
      view_.err = ErrCode::ShtUnknown;
      view_.h = Health::Fault;
      lostSinceFound_ = everFound_;
      scheduleReprobe(now);
      break;
    default:
      health_.set(Device::Sht, Health::Absent, ErrCode::ShtNone, now);
      log_.log(LogLevel::E, "SHT", "E-SHT-NONE nothing at 0x44/0x45 (SDA8 SCL7)");
      view_.err = ErrCode::ShtNone;
      view_.h = Health::Absent;
      lostSinceFound_ = everFound_;
      scheduleReprobe(now);
      break;
  }
}

void ThermoService::publish(MonoUs now, const ThermoCtx& c) {
  const float t = tRaw_ + c.offsetTenths / 10.0f;
  const float rh = c.offsetTenths ? correctRhForTemp(rhRaw_, tRaw_, t) : rhRaw_;   // dew point kept (§6.2)
  view_.tC = t;
  view_.rh = rh;
  view_.dewC = dewPointC(t, rh);
  view_.ahGm3 = absHumidity(t, rh);
  view_.valid = true;
  view_.stale = false;
  haveValid_ = true;
  lastValid_ = now;
  highRhStreak_ = rh >= kHeaterAutoRh ? (uint8_t)(highRhStreak_ < 255 ? highRhStreak_ + 1 : 255) : 0;
}

void ThermoService::onSample(MonoUs now, const ShtSample& s, const ThermoCtx& c) {
  if (s.hot) {   // never published; logged and kept for Diagnostics
    heatPre_ = preT_;
    heatHot_ = s.tC;
    haveHeat_ = true;
    log_.log(LogLevel::I, "SHT", "heat pre=%.1f hot=%.1f d=%+.1f", (double)preT_, (double)s.tC, (double)(s.tC - preT_));
    view_.lastHotC = s.tC;
    view_.lastPreC = preT_;
    heaterUi_ = 2;
    coolN_ = 0;
    return;
  }
  tRaw_ = s.tC;
  rhRaw_ = s.rh;
  if (heaterUi_ == 2) {   // cool-down: publish again only when the reading settled (§6.2)
    const int64_t since = now - coolStart_;
    if (coolN_ == 6) { for (int i = 1; i < 6; ++i) { coolT_[i - 1] = coolT_[i]; coolAt_[i - 1] = coolAt_[i]; } --coolN_; }
    coolT_[coolN_] = s.tC; coolAt_[coolN_] = now; ++coolN_;
    bool settled = false;
    if (since >= ms(kHeaterCoolMinMs) && c.displayOn) {
      if (fabsf(s.tC - preT_) <= 0.3f) settled = true;
      for (uint8_t i = 0; i < coolN_ && !settled; ++i) {
        const int64_t dt = now - coolAt_[i];
        if (dt >= 10 * kUsPerS && fabsf((s.tC - coolT_[i]) / (float)(dt / 1e6)) < 0.05f) settled = true;
      }
    }
    if (since >= ms(kHeaterCoolMaxMs)) settled = true;
    if (!settled) return;
    heaterUi_ = 0;
  }
  publish(now, c);
}

void ThermoService::onError(MonoUs now) {
  log_.log(LogLevel::W, "SHT", "read error (crc %u io %u, %u in a row)", drv_.crcErrors(), drv_.ioErrors(),
           drv_.consecutiveErrors());
  if (heaterUi_ == 1) heaterUi_ = 2;   // a lost hot sample must never let hot values be published
  if (drv_.consecutiveErrors() >= 3) {
    const ErrCode e = drv_.crcErrors() > drv_.ioErrors() ? ErrCode::ShtCrc : ErrCode::ShtIo;
    health_.set(Device::Sht, Health::Fault, e, now);
    log_.log(LogLevel::E, "SHT", "%s 3 failed reads, re-probing", e == ErrCode::ShtCrc ? "E-SHT-CRC" : "E-SHT-IO");
    view_.err = e;
    view_.h = Health::Fault;
    lostSinceFound_ = true;
    scheduleReprobe(now);
  }
}

bool ThermoService::heaterTest(MonoUs now, const ThermoCtx& c, HeaterDecision* why) {
  // A test pulse is always "due"; the guards stay: SHT4x only, not busy, 60 s spacing, T <= 60 C, no shutdown.
  const HeaterIn in{HeaterMode::Every5, drv_.chip() == ShtChip::Sht4x, busy(), now, false, lastPulse_,
                    view_.valid, view_.tC, highRhStreak_, c.shutdownPending};
  const HeaterDecision d = havePulse_ && now - lastPulse_ < ms(kHeaterMinSpacingMs) ? HeaterDecision::SkipSpacing : heaterDue(in);
  if (why) *why = d;
  if (d != HeaterDecision::Pulse) return false;
  preT_ = tRaw_;
  if (!drv_.startHeater(now, heaterCommand(c.power))) return false;
  heaterAck_ = true;
  havePulse_ = true;
  lastPulse_ = coolStart_ = now;
  heaterUi_ = 1;
  return true;
}

void ThermoService::tick(MonoUs now, const ThermoCtx& c) {
  ShtSample s{};
  const ShtDriver::Poll p = drv_.tick(now, &s);
  if (wasProbing_ && !drv_.probing()) {
    wasProbing_ = false;
    onProbeDone(now);
    if (p == ShtDriver::Poll::Sample) onSample(now, s, c);   // the cached-chip check returns a measurement
  } else if (p == ShtDriver::Poll::Sample) {
    onSample(now, s, c);
  } else if (p == ShtDriver::Poll::Error) {
    onError(now);
  }
  view_.heaterUi = heaterUi_;
  view_.lastSkip = lastSkip_;
  view_.stale = c.displayOn && haveValid_ && now - lastValid_ >= ms(kShtStaleMs);
  if (drv_.probing() || drv_.busy()) return;
  const bool ok = drv_.chip() == ShtChip::Sht4x || drv_.chip() == ShtChip::Sht3x;
  if (!ok || view_.h == Health::Fault) {
    if (c.displayOn && now.v >= nextProbe_.v) beginProbe(now, ShtProbeHint{});
    return;
  }
  // heater (scheduled pulses run while dark too)
  if (c.mode != HeaterMode::Off && heaterUi_ != 1) {
    HeaterIn in{c.mode, drv_.chip() == ShtChip::Sht4x, false, now, havePulse_, lastPulse_, view_.valid, view_.tC,
                highRhStreak_, c.shutdownPending};
    const HeaterDecision d = heaterDue(in);
    if (d == HeaterDecision::Pulse && heaterUi_ == 0) {
      preT_ = tRaw_;
      if (drv_.startHeater(now, heaterCommand(c.power))) {
        heaterAck_ = true;
        havePulse_ = true;
        lastPulse_ = coolStart_ = now;
        heaterUi_ = 1;
        lastSkip_ = HeaterDecision::No;
        log_.log(LogLevel::I, "SHT", "heater pulse 0x%02X", heaterCommand(c.power));
      }
      return;
    }
    if (d == HeaterDecision::SkipHot) {
      lastSkip_ = d;
      if (!skipHotShown_) { skipHotShown_ = true; skipHotEvent_ = true; log_.log(LogLevel::W, "SHT", "W-HEAT-HOT heater skipped, T > 60 C"); }
      havePulse_ = true;   // try again one period later, not every loop
      lastPulse_ = now;
    }
  }
  // measurement cadence: the period of NOW counts from the last start, so a faster page takes effect at once
  int64_t period = 0;
  if (c.displayOn) period = ms(c.fast ? kShtHomePeriodMs : kShtOnPeriodMs);
  else if (c.mode == HeaterMode::Auto) period = ms(kShtAutoDarkPeriodMs);
  if (measureNow_ || (period && (!measuredOnce_ || now - lastMeasure_ >= period))) {
    measureNow_ = false;
    if (drv_.startMeasure(now)) { lastMeasure_ = now; measuredOnce_ = true; }
  }
}

MonoUs ThermoService::nextDue(MonoUs now, const ThermoCtx& c) const {
  if (busy()) return now + 5 * kUsPerMs;
  MonoUs due{INT64_MAX};
  auto earlier = [&](MonoUs t) { if (t.v < due.v) due = t; };
  if (c.displayOn || c.mode == HeaterMode::Auto)
    earlier(lastMeasure_ + ms(c.displayOn ? (c.fast ? kShtHomePeriodMs : kShtOnPeriodMs) : kShtAutoDarkPeriodMs));
  if (heaterUi_ == 2) earlier(coolStart_ + ms(kHeaterCoolMaxMs));
  if (c.mode >= HeaterMode::Every5 && havePulse_) {
    static const int64_t kPeriodMin[6] = {0, 0, 5, 10, 30, 60};
    earlier(lastPulse_ + kPeriodMin[(int)c.mode] * 60 * kUsPerS);
  } else if (c.mode >= HeaterMode::Every5) {
    earlier(now);
  }
  return due;
}

}  // namespace hg
