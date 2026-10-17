#include "app_core.h"

#include <stdio.h>
#include <string.h>

#include "../config/board_pins.h"
#include "../config/build_config.h"
#include "../config/timing.h"
#include "../config/version.h"
#include "../core/cli.h"
#include "../core/crc.h"
#include "../core/rtc_bridge.h"
#include "../ui/fmt.h"
#include "../ui/strings.h"
#include "console.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }

// ext1 GPIO mask <-> logical key bits
uint8_t keysFromGpioMask(uint64_t m) {
  uint8_t k = 0;
  for (uint8_t i = 0; i < kKeyCount; ++i) if (m & (1ULL << kKeyPins[i])) k |= (uint8_t)(1u << i);
  return k;
}
uint64_t gpioMaskFromKeys(uint8_t keys) {
  uint64_t m = 0;
  for (uint8_t i = 0; i < kKeyCount; ++i) if (keys & (1u << i)) m |= 1ULL << kKeyPins[i];
  return m;
}
uint8_t firstKey(uint8_t mask) { for (uint8_t i = 0; i < kKeyCount; ++i) if (mask & (1u << i)) return i; return 0; }
}  // namespace

AppCore::AppCore(const Ports& p)
    : p_(p),
      log_(p.serial, p.mono),
      persist_(p.kv),
      time_(p.wall, persist_, log_),
      thermo_(p.sensorBus, health_, log_),
      compass_(p.sensorBus, health_, log_),
      power_(p.power, p.display, log_),
      canvas_(p.display->u8g2()),
      ui_(canvas_, *p.display) {
  settings_ = defaultSettings();
  health_.setSink(
      [](void* ctx, Device d, const DeviceHealth& before, const DeviceHealth& after) {
        Logger* l = static_cast<Logger*>(ctx);
        if (after.code == before.code && after.h == before.h) return;
        const ErrInfo& e = errInfo(after.code);
        const LogLevel lvl = after.h == Health::Ok ? LogLevel::I : e.sev >= Severity::Fault ? LogLevel::E : LogLevel::W;
        l->log(lvl, deviceName(d), "health %s %s", after.h == Health::Ok ? "OK" : after.h == Health::Absent ? "ABSENT"
               : after.h == Health::Fault ? "FAULT" : "DEGRADED", e.mnemonic);
      },
      &log_);
}

const ViewModel& AppCore::viewModel() const { return vm_; }
const TzDef& AppCore::tz() const { return kTzTable[sanitize(SettingId::TimeZone, settings_.get(SettingId::TimeZone))]; }

Settings AppCore::liveSettings() const {
  Settings s = settings_;
  if (ui_.modalOpen(ScreenId::Picker)) s.set(ui_.state().listCursor, ui_.state().pending);
  return s;
}

Brightness AppCore::brightness() const { return (Brightness)optionValue(SettingId::Brightness, liveSettings().get(SettingId::Brightness)); }
uint32_t AppCore::screenTimeoutMs() const { return (uint32_t)optionValue(SettingId::ScreenOff, settings_.get(SettingId::ScreenOff)) * 1000u; }
uint32_t AppCore::autoOffMs() const { return (uint32_t)optionValue(SettingId::AutoOff, settings_.get(SettingId::AutoOff)) * 60000u; }

ThermoCtx AppCore::thermoCtx(bool fast) const {
  const Settings s = liveSettings();
  return ThermoCtx{power_.displayOn(), fast, s.get(SettingId::TempOffset), (HeaterMode)optionValue(SettingId::Heater, s.get(SettingId::Heater)),
                   (HeaterPower)optionValue(SettingId::HeaterPower, s.get(SettingId::HeaterPower)), shut_ != Shut::Idle};
}

CompassCtx AppCore::compassCtx(bool demand) const {
  const Settings s = liveSettings();
  return CompassCtx{demand, (uint8_t)optionValue(SettingId::Mount, s.get(SettingId::Mount)), s.get(SettingId::Declination),
                    (uint8_t)optionValue(SettingId::NeedleSpeed, s.get(SettingId::NeedleSpeed))};
}

void AppCore::nvsFailed(const char* key, MonoUs now) {
  log_.log(LogLevel::E, "NVS", "E-NVS put '%s' failed", key);
  health_.set(Device::Nvs, Health::Fault, ErrCode::NvsFail, now);
  if (!nvsCardShown_) { nvsCardShown_ = true; ui_.showCard(ErrCode::NvsFail); }
}

void AppCore::scanBuses() {
  n0_ = p_.sensorBus ? p_.sensorBus->scan(scan0_, sizeof scan0_) : 0;
  n1_ = p_.oledBus ? p_.oledBus->scan(scan1_, sizeof scan1_) : 0;
  char b[64] = "";
  for (uint8_t i = 0; i < n0_; ++i) { char t[5]; snprintf(t, sizeof t, " %02X", scan0_[i]); strncat(b, t, sizeof b - strlen(b) - 1); }
  log_.log(LogLevel::I, "I2C0", "SDA8 SCL7 100k scan:%s", n0_ ? b : " -");
  b[0] = 0;
  for (uint8_t i = 0; i < n1_; ++i) { char t[5]; snprintf(t, sizeof t, " %02X", scan1_[i]); strncat(b, t, sizeof b - strlen(b) - 1); }
  log_.log(LogLevel::I, "I2C1", "SDA10 SCL11 400k scan:%s", n1_ ? b : " -");
}

// ---------------------------------------------------------------- boot (§4.6 steps 5-12)
void AppCore::boot(const BootInputs& in) {
  bootIn_ = in;
  const MonoUs now = p_.mono->now();
  bootAt_ = now;
  wake_ = p_.power->wakeInfo();
  RtcState& rs = p_.rtcStore->state();
  verdict_ = decideRestore(wake_.resetReason, rs);
  const bool rtcOk = verdict_ == RestoreVerdict::Restore || verdict_ == RestoreVerdict::RestoreCrash;
  rtcWasValid_ = rtcOk;
  const RtcUs rtcNow = p_.rtc->now();
  const int64_t slept = rtcVerify(rs) || rtcMagicOnly(rs) ? sleptUs(rs, rtcNow) : 0;
  log_.log(LogLevel::I, "BOOT", "Hiking Gadget %s reset %u wake %u verdict %u", kFwVersion, (unsigned)wake_.resetReason,
           (unsigned)wake_.cause, (unsigned)verdict_);
  // settings (NVS namespace hg2)
  if (!persist_.ok()) {
    health_.set(Device::Nvs, Health::Fault, ErrCode::NvsFail, now);
    log_.log(LogLevel::E, "NVS", "E-NVS storage unavailable: settings live in RAM only");
  } else {
    const Persist::LoadResult lr = persist_.loadSettings(settings_);
    if (lr.anyInvalid) {
      health_.set(Device::Nvs, Health::Degraded, ErrCode::CfgReset, now);
      log_.log(LogLevel::W, "CFG", "W-CFG-RESET '%s' had invalid value %d, reset (%u key(s))", settingDef(lr.firstInvalid).nvsKey,
               lr.firstOld, lr.invalidCount);
      ui_.showCard(ErrCode::CfgReset, (uint8_t)lr.firstInvalid);
    }
    persist_.bumpBoots();
    uint8_t ring[8];
    persist_.pushResetReason((uint8_t)wake_.resetReason, ring);
  }
  ui_.stateForTest().animations = optionValue(SettingId::Animations, settings_.get(SettingId::Animations)) != 0;
  // clock (§5.3)
  const BootClockOut clk = time_.boot(in.buildDate, in.buildTime, rtcOk, rs.clock);
  if (clk.clockLost) {
    health_.set(Device::Clock, Health::Degraded, ErrCode::ClockLost, now);
    ui_.toast(S_T_CLOCK_LOST, nullptr, now);
  } else if (time_.meta().q == ClockQuality::Approx) {
    ui_.toast(S_T_CLOCK_APPROX, nullptr, now);
  }
  // models (§5.4)
  tm_.setPreset(persist_.loadPreset());
  prevSht_ = rtcOk ? rs.shtChip : ShtChip::None;
  switch (verdict_) {
    case RestoreVerdict::Restore:
    case RestoreVerdict::RestoreCrash:
      sw_.restore(rs.sw, slept, now);
      tm_.restore(rs.tm, slept, now);
      compass_.restoreTarget(rs.targetDeg);
      ui_.stateForTest().page = (uint8_t)(rs.page % UiController::kPages);
      missedBell_ = (rs.alertFlags & 2) != 0;
      if ((rs.alertFlags & 1) && tm_.state() == Countdown::State::Done) {
        alertActive_ = true;
        alertAt_ = MonoUs{now.v - tm_.overtimeUs(now)};
        ui_.raiseAlert(now);
      }
      if (verdict_ == RestoreVerdict::RestoreCrash) {
        ++rs.crashCount;
        health_.set(Device::Power, Health::Degraded, ErrCode::CrashRestored, now);
        ui_.toast(S_T_CRASH, nullptr, now);
      }
      if (rs.buildId != time_.buildId()) log_.log(LogLevel::I, "BOOT", "restored across update");
      break;
    case RestoreVerdict::Lost:
      health_.set(Device::Power, Health::Fault, ErrCode::RtcLost, now);
      log_.log(LogLevel::E, "BOOT", "E-RTC-LOST sleep memory unusable: models reset");
      ui_.showCard(ErrCode::RtcLost, wake_.cause == WakeCause::Timer ? 1 : 0);
      rs.crashCount = 0;
      break;
    case RestoreVerdict::CrashLoop:
      health_.set(Device::Power, Health::Degraded, ErrCode::CrashRestored, now);
      log_.log(LogLevel::W, "BOOT", "W-RST-CRASH 3 crashes in a row: state not restored");
      ui_.showCard(ErrCode::RtcLost, 2);
      rs.crashCount = 0;
      break;
    default:
      if (wake_.resetReason == ResetReason::Brownout) health_.set(Device::Power, Health::Degraded, ErrCode::Brownout, now);
      memset(&rs, 0, sizeof rs);
      rs.targetDeg = -1;
      break;
  }
  // display at boot (§4.6 step 7): cold -> splash, EXT1 -> last page, TIMER -> dark unless the timer state is lost
  const bool timerWake = wake_.cause == WakeCause::Timer;
  bool displayOn = true;
  if (timerWake) {
    const bool restored = verdict_ == RestoreVerdict::Restore || verdict_ == RestoreVerdict::RestoreCrash;
    const bool timerBack = tm_.state() == Countdown::State::Running || tm_.state() == Countdown::State::Done;
    displayOn = !(restored && timerBack);
    if (restored && !timerBack) {
      ui_.showCard(ErrCode::RtcLost, 1);
      health_.set(Device::Power, Health::Fault, ErrCode::RtcLost, now);
      log_.log(LogLevel::E, "BOOT", "E-RTC-LOST timer wake without a running timer");
    }
  }
  splash_ = wake_.cause == WakeCause::None && wake_.resetReason != ResetReason::DeepSleep;
  if (splash_) ui_.showSplash(true);
  // keys stuck at the last deep sleep
  if (rtcOk && rs.stuckKeyMask) {
    ui_.showCard(ErrCode::KeyStuck, (uint8_t)(firstKey(rs.stuckKeyMask) | (1 << 4)));
    health_.set(Device::Keys, Health::Degraded, ErrCode::KeyStuck, now);
  }
  // sensors
  ShtProbeHint hint{};
  if (rtcOk && wake_.resetReason == ResetReason::DeepSleep && (rs.shtChip == ShtChip::Sht4x || rs.shtChip == ShtChip::Sht3x)) {
    hint.cached = rs.shtChip;
    hint.addr = rs.shtAddr;
  }
  if ((rtcVerify(rs)) && rs.shtBusyUntil.v > rtcNow.v) hint.busyWaitUs = (uint32_t)(rs.shtBusyUntil - rtcNow);
  thermo_.beginProbe(now, hint);
  compass_.probe(now);
  MagCal cal{};
  if (persist_.loadCal(cal, nullptr)) compass_.applyCalibration(cal);
  scanBuses();
  // power
  power_.begin(now, displayOn, brightness());
  lastDisplayOn_ = displayOn;
  displayOk_ = p_.display->ok();
  if (!displayOk_) {
    health_.set(Device::Oled, Health::Fault, ErrCode::OledNone, now);
    log_.log(LogLevel::E, "OLED", "E-OLED-NONE no ACK at 0x3C/0x3D on SDA10/SCL11: running headless");
  }
  hostWasConnected_ = p_.serial && p_.serial->hostConnected();   // a host present at boot gets the boot report, no replay
  // keys LOW at boot are swallowed until released (§2.3.2); the woken key that is already up is not
  input_.feed(wake_.levelsAtBoot, now);
  input_.swallowUntilRelease(wake_.levelsAtBoot);
  // phantom-wake guard (§7.5)
  if (wake_.cause == WakeCause::Ext1 && (rtcVerify(rs) || rtcMagicOnly(rs))) {
    const uint8_t woken = keysFromGpioMask(wake_.ext1Mask);
    if (isPhantomWake(WakeCause::Ext1, woken, wake_.levelsAtBoot, slept)) {
      log_.log(LogLevel::W, "KEY", "phantom wake by mask %02X after %lld ms", woken, (long long)(slept / 1000));
      if (countPhantomWake(rs, rtcNow)) {
        rs.stuckKeyMask |= woken;
        rs.phantomWakes = 0;
        ui_.showCard(ErrCode::KeyStuck, (uint8_t)(firstKey(woken) | (2 << 4)));
        health_.set(Device::Keys, Health::Degraded, ErrCode::KeyStuck, now);
        log_.log(LogLevel::E, "KEY", "E-KEY-STUCK %u phantom wakes: excluded from wake", kPhantomWakeLimit);
      }
    }
  }
  buildViewModel(now);   // the boot report starts once the probes are done (tickSensors), so it names the sensors
  snapshot(now);
}

// ---------------------------------------------------------------- RtcState snapshot (§5.4)
void AppCore::snapshot(MonoUs now) {
  RtcState& rs = p_.rtcStore->state();
  const uint8_t crash = rtcWasValid_ ? rs.crashCount : 0;
  const uint8_t phantom = rs.phantomWakes;
  const RtcUs pws = rs.phantomWindowStart;
  const uint8_t stuck = rs.stuckKeyMask;
  const uint8_t lastSleep = rs.lastSleep;
  const RtcUs busy = rs.shtBusyUntil;
  memset(&rs, 0, sizeof rs);
  rs.buildId = time_.buildId();
  rs.clock = time_.meta();
  rs.savedAt = p_.rtc->now();
  rs.sw = sw_.save(now);
  rs.tm = tm_.save(now);
  rs.targetDeg = compass_.targetSet() ? compass_.target() : (int16_t)-1;
  rs.page = ui_.state().page;
  rs.alertFlags = (uint8_t)((alertActive_ ? 1 : 0) | (missedBell_ ? 2 : 0));
  rs.lastSleep = lastSleep;
  rs.stuckKeyMask = stuck;
  rs.phantomWakes = phantom;
  rs.phantomWindowStart = pws;
  rs.crashCount = crash;
  rs.shtChip = thermo_.chip();
  rs.shtAddr = thermo_.addr();
  rs.shtBusyUntil = busy;
  rtcSeal(rs);
  rtcWasValid_ = true;
  snapRev_ = modelRev_;
}

// ---------------------------------------------------------------- keys (§4.5 step 1)
void AppCore::handleKeys(MonoUs now) {
  Bindings b;
  ui_.bindings(vm_, b);
  for (uint8_t k = 0; k < kKeyCount; ++k) input_.configure((Key)k, keyConfigFor(b.key[k]));
  input_.setCaptureAll(b.captureAll);
  input_.feed(p_.keys->rawPressedMask(), now);
  InputEvent e;
  while (input_.pop(e)) {
    const uint8_t k = (uint8_t)e.key;
    if (e.type == InputEvent::Type::Stuck) {
      log_.log(LogLevel::W, "KEY", "K%u (GPIO%u) held 10 s, ignored", k + 1u, kKeyPins[k]);
      health_.set(Device::Keys, Health::Degraded, ErrCode::KeyStuck, now);
      ui_.showCard(ErrCode::KeyStuck, k);
      power_.wake(now, brightness());
      continue;
    }
    if (e.type == InputEvent::Type::Unstuck) {
      log_.log(LogLevel::I, "KEY", "K%u works again", k + 1u);
      if (!input_.stuckMask()) health_.set(Device::Keys, Health::Ok, ErrCode::None, now);
      ui_.toast(S_T_KEY_OK, k == 1 ? "v" : (k == 0 ? "^" : k == 2 ? "#" : "*"), now);
      continue;
    }
    power_.activity(now);
    if (e.type == InputEvent::Type::Down) {
      if (keyPresses_[k] < 65535) ++keyPresses_[k];
      p_.rtcStore->state().phantomWakes = 0;   // any accepted press resets the phantom count
      log_.log(LogLevel::D, "KEY", "%c down t=%lld", "^v#*"[k], (long long)(e.tDown.v / 1000));
      if (!power_.displayOn() || power_.display() == DisplayState::Dim) {   // wake swallow (§2.3.1)
        input_.swallowUntilRelease(keyBit(e.key));
        power_.wake(now, brightness());
        ui_.forceFullRefresh();
        continue;
      }
      if (alertActive_ && !b.captureAll) {                             // any key dismisses the alert
        input_.swallowUntilRelease(keyBit(e.key));
        dismissAlert(now);
        continue;
      }
    }
    const UiCommand c = ui_.dispatch(e, b, vm_, now);
    if (c.action != ActionId::None) act(c, now);
  }
  ui_.setHold(input_.hold(now));
}

// ---------------------------------------------------------------- actions
void AppCore::toastTimerLeft(MonoUs now) {
  char t[16];
  const int64_t s = Countdown::displaySeconds(tm_.remainingUs(now));
  if (s >= 3600) fmtHMmSs(t, sizeof t, s);
  else snprintf(t, sizeof t, "%d:%02d", (int)(s / 60), (int)(s % 60));
  ui_.toast(S_T_PLUS1, t, now);
}

bool AppCore::saveSetting(SettingId id, int16_t v, MonoUs now) {
  settings_.set(id, v);
  applySettingSideEffects(id);
  if (!persist_.saveSetting(id, v)) {
    nvsFailed(settingDef(id).nvsKey, now);
    ui_.toast(S_T_NOT_STORED, nullptr, now);
    return false;
  }
  return true;
}

void AppCore::applySettingSideEffects(SettingId id) {
  if (id == SettingId::Animations) ui_.stateForTest().animations = optionValue(id, settings_.get(id)) != 0;
}

void AppCore::act(const UiCommand& c, MonoUs now) {
  ++modelRev_;
  switch (c.action) {
    case ActionId::DisplayOff: power_.displayOffNow(now, screenTimeoutMs()); break;
    case ActionId::PowerOff: startShutdown(now, true); break;
    case ActionId::SwStart: sw_.start(c.tDown); break;
    case ActionId::SwPause: sw_.pause(c.tDown); break;
    case ActionId::SwResume: sw_.resume(c.tDown); break;
    case ActionId::SwReset: sw_.reset(); break;
    case ActionId::SwLap:
      if (!sw_.lap(c.tDown)) ui_.toast(S_T_MAX_LAPS, nullptr, now);
      else log_.log(LogLevel::I, "SW", "LAP %u split %lld lap %lld", sw_.lapNo(), (long long)(sw_.elapsedUs(c.tDown) / 1000),
                    (long long)(sw_.lapUs(0) / 1000));
      break;
    case ActionId::TmNextPreset: tm_.nextPreset(); break;
    case ActionId::TmStart:
      tm_.start(c.tDown);
      if (!persist_.savePreset(tm_.preset())) { nvsFailed("tmpre", now); ui_.toast(S_T_NOT_STORED, nullptr, now); }
      log_.log(LogLevel::I, "TMR", "start %d min", Countdown::kPresetMin[tm_.preset()]);
      break;
    case ActionId::TmPause: tm_.pause(c.tDown); break;
    case ActionId::TmResume: tm_.resume(c.tDown); break;
    case ActionId::TmAddMinute:
      if (tm_.addMinute()) toastTimerLeft(now);
      else ui_.toast(S_T_MAX_10H, nullptr, now);
      break;
    case ActionId::TmCancel: tm_.cancel(); break;
    case ActionId::TmNew: tm_.toSelect(); break;
    case ActionId::TmRestart: tm_.restart(c.tDown); break;
    case ActionId::CmpMark: {
      int16_t b = 0;
      if (compass_.markTarget(&b)) { char t[8]; snprintf(t, sizeof t, "%03d`", b); ui_.toast(S_T_TARGET, t, now); }
      else ui_.toast(S_T_NO_HEADING, nullptr, now);
      break;
    }
    case ActionId::CmpClear: compass_.clearTarget(); ui_.toast(S_T_TARGET_CLEARED, nullptr, now); break;
    case ActionId::CmpCalibrate: compass_.beginCalibration(now); break;
    case ActionId::CalCancel:
      if (compass_.calibrating()) ui_.toast(S_T_NOT_SAVED, nullptr, now);
      compass_.cancelCalibration();
      break;
    case ActionId::CalRetry: compass_.retryCalibration(now); break;
    case ActionId::CalSave: {
      MagCal cal;
      if (compass_.saveCalibration(&cal) && !persist_.saveCal(cal, time_.utcS())) {
        nvsFailed("cal", now);
        ui_.toast(S_T_NOT_STORED, nullptr, now);
      }
      break;
    }
    case ActionId::CalDone: compass_.cancelCalibration(); break;
    case ActionId::SettingSave: {
      const SettingId id = UiController::savedSetting(c);
      if (saveSetting(id, UiController::savedValue(c), now)) ui_.toast(S_T_SAVED, nullptr, now);
      break;
    }
    case ActionId::SetTimeCommit: {
      int32_t d = 0;
      const SyncVerdict v = time_.setLocal(ui_.state().timeEdit, tz(), ClockSource::Manual, &d);
      if (v == SyncVerdict::Ok) {
        health_.set(Device::Clock, Health::Ok, ErrCode::None, now);
        ui_.toast(S_T_TIME_SET, nullptr, now);
        log_.log(LogLevel::I, "CLK", "time set by hand (delta %+ld ms)", (long)d);
      } else {
        ui_.toast(S_T_SYNC_REJ, "range", now);
      }
      break;
    }
    case ActionId::ResetSettings:
      persist_.resetSettings();
      log_.log(LogLevel::W, "CFG", "settings reset by the user, rebooting");
      snapshot(now);
      p_.power->restart();
      break;
    case ActionId::Reboot:
      log_.log(LogLevel::I, "PWR", "reboot requested");
      snapshot(now);
      p_.power->restart();
      break;
    case ActionId::HeaterTest: {
      HeaterDecision why;
      if (thermo_.heaterTest(now, thermoCtx(false), &why)) ui_.toast(S_T_HEAT_TEST, nullptr, now);
      else ui_.toast(why == HeaterDecision::SkipHot ? S_T_HEAT_HOT : S_T_HEAT_NA, nullptr, now);
      break;
    }
    case ActionId::Rescan:
      compass_.rescan(now);
      if (thermo_.chip() == ShtChip::None || thermo_.chip() == ShtChip::Unknown) thermo_.beginProbe(now, ShtProbeHint{});
      scanBuses();
      ui_.toast(S_T_RESCANNED, nullptr, now);
      break;
    case ActionId::MagMinMaxReset: compass_.resetMinMax(); ui_.toast(S_T_MINMAX_RESET, nullptr, now); break;
    case ActionId::AlertDismiss: dismissAlert(now); break;
    default: break;
  }
}

// ---------------------------------------------------------------- alert (§2.3.4)
void AppCore::raiseAlert(MonoUs now) {
  alertActive_ = true;
  alertAt_ = now;
  lastFlip_ = now;
  ui_.raiseAlert(now);
  power_.forceHigh(true);
  power_.wake(now, Brightness::High);
  log_.log(LogLevel::I, "TMR", "timer done: alert");
  ++modelRev_;
}

void AppCore::dismissAlert(MonoUs now) {
  if (!alertActive_) return;
  alertActive_ = false;
  if (invertOn_ && p_.display) p_.display->setInvert(false);
  invertOn_ = false;
  power_.forceHigh(false);
  power_.activity(now);
  ui_.dismissAlert(now);
  missedBell_ = false;
  ++modelRev_;
  if (deferredOff_) { deferredOff_ = false; startShutdown(now, true); }
}

void AppCore::tickAlert(MonoUs now) {
  if (!alertActive_) return;
  const int64_t t = now - alertAt_;
  power_.activity(now);   // the alert has its own timers (§2.3.3)
  if (t < ms(kAlertFlashMs)) {
    if (now - lastFlip_ >= ms(kAlertFlashPeriodMs)) {
      lastFlip_ = now;
      invertOn_ = !invertOn_;
      if (p_.display) p_.display->setInvert(invertOn_);
    }
    return;
  }
  if (invertOn_) { invertOn_ = false; if (p_.display) p_.display->setInvert(false); }
  if (t >= ms(kAlertHoldMs)) {   // phase B over: display off, a missed-alert bell stays
    alertActive_ = false;
    power_.forceHigh(false);
    missedBell_ = true;
    power_.displayOffNow(now, screenTimeoutMs());
    ui_.dismissAlert(now);
    log_.log(LogLevel::I, "TMR", "alert not seen: bell on the status line");
    ++modelRev_;
    if (deferredOff_) { deferredOff_ = false; startShutdown(now, false); }
  }
}

// ---------------------------------------------------------------- models (§4.5 step 2)
void AppCore::tickModels(MonoUs now) {
  if (sw_.tick(now)) { ui_.toast(S_T_MAX_TIME, nullptr, now); ++modelRev_; }
  if (tm_.tick(now)) raiseAlert(now);
  tickAlert(now);
  // the missed-alert bell clears when the Timer page is seen
  if (missedBell_ && power_.displayOn() && ui_.topScreen(vm_) == ScreenId::Timer) { missedBell_ = false; ++modelRev_; }
  const uint8_t page = ui_.state().page;
  if (page != p_.rtcStore->state().page) ++modelRev_;
  if (modelRev_ != snapRev_) snapshot(now);
  if (!crashCleared_ && now - bootAt_ >= ms(kCrashRestoreUptimeMs)) {   // 60 s up: the crash counter resets
    crashCleared_ = true;
    if (p_.rtcStore->state().crashCount) { p_.rtcStore->state().crashCount = 0; snapshot(now); }
  }
}

// ---------------------------------------------------------------- sensors (§4.5 step 3)
void AppCore::tickSensors(MonoUs now) {
  const uint8_t demand = power_.displayOn() ? ui_.sensorDemand(vm_) : 0;
  const bool homeFast = (demand & 2) != 0;
  thermo_.tick(now, thermoCtx(homeFast));
  if (thermo_.takeHeaterAck()) {   // a reset during the pulse must wait for it (§6.2)
    p_.rtcStore->state().shtBusyUntil = RtcUs{p_.rtc->now().v + kShtHeaterBusyMaxMs * 1000ULL};
    ++modelRev_;
  }
  if (thermo_.takeProbeDone()) {
    if (thermo_.chip() == ShtChip::Sht3x && prevSht_ != ShtChip::Sht3x) ui_.showCard(ErrCode::Sht3xInfo);
    prevSht_ = thermo_.chip();
    ++modelRev_;
  }
  if (thermo_.takeBack()) ui_.toast(S_T_TEMP_BACK, nullptr, now);
  if (thermo_.takeSkipHot()) ui_.toast(S_T_HEAT_HOT, nullptr, now);
  const bool magDemand = (demand & 1) != 0 || now.v < magStreamUntil_.v;
  compass_.tick(now, compassCtx(magDemand));
  if (compass_.takeBack()) ui_.toast(S_T_COMPASS_BACK, nullptr, now);
  if (now.v < magStreamUntil_.v) {
    MagSample s;
    if (compass_.samples() != magStreamN_ && compass_.lastSample(&s)) {
      magStreamN_ = compass_.samples();
      log_.reply("%lld,%d,%d,%d,%d", (long long)(s.at.v / 1000), s.x, s.y, s.z, s.overflow ? 1 : 0);
    }
  }
  if (p_.battery && (now - lastBattery_ >= 30 * kUsPerS || !lastBattery_.v)) { bat_ = p_.battery->read(); lastBattery_ = now; }
  // DRDY edge counter: attached only while Diagnostics > Sensors is open (§4.2 rule 4)
  const bool wantDrdy = power_.displayOn() && ui_.topScreen(vm_) == ScreenId::Diagnostics && ui_.state().diagPage == 0;
  if (p_.drdy && wantDrdy != drdyOn_) {
    drdyOn_ = wantDrdy;
    if (wantDrdy) { p_.drdy->enable(compass_.view().drdy); drdyLast_ = p_.drdy->edges(); drdyAt_ = now; }
    else p_.drdy->disable();
  }
  if (drdyOn_ && now - drdyAt_ >= kUsPerS) { drdyRate_ = p_.drdy->edges() - drdyLast_; drdyLast_ = p_.drdy->edges(); drdyAt_ = now; }
  // sensor bus: a recovery is a warning; a line still held after recovery is the FATAL card (§6.1)
  if (p_.sensorBus) {
    const I2cStats& st = p_.sensorBus->stats();
    if (st.recoveryFails != busFail_) {
      busFail_ = st.recoveryFails;
      health_.set(Device::Bus0, Health::Fault, ErrCode::I2cStuck, now);
      log_.log(LogLevel::E, "I2C0", "E-I2C-STUCK SDA8 held low after 9 clocks");
      ui_.showCard(ErrCode::I2cStuck);
      power_.wake(now, brightness());
    } else if (st.recoveries != busRec_) {
      busRec_ = st.recoveries;
      health_.set(Device::Bus0, Health::Degraded, ErrCode::I2cRecovered, now);
      log_.log(LogLevel::W, "I2C0", "W-I2C-REC bus recovered");
    }
  }
  // boot report + CHECK HARDWARE once the boot probes are done (§4.6 steps 9-11), at most 1.5 s after boot
  if (!checkHwShown_ && (!thermo_.probing() || now - bootAt_ >= ms(kSplashMaxMs))) {
    checkHwShown_ = true;
    buildViewModel(now);
    logStartBootReport(*this);
    const ErrCode first = health_.get(Device::Bus0).code != ErrCode::None && errInfo(health_.get(Device::Bus0).code).sev >= Severity::Fault ? health_.get(Device::Bus0).code
                        : thermo_.view().err != ErrCode::None ? thermo_.view().err
                        : compass_.view().err != ErrCode::None && errInfo(compass_.view().err).sev >= Severity::Fault ? compass_.view().err
                        : health_.get(Device::Nvs).code == ErrCode::NvsFail ? ErrCode::NvsFail : ErrCode::None;
    if (first != ErrCode::None && errInfo(first).sev >= Severity::Fault) ui_.showCard(first);
  }
  // at runtime: a subsystem that was OK and fails gets the CHECK HARDWARE card (once per failure)
  if (checkHwShown_) {
    const Device devs[4] = {Device::Sht, Device::Mag, Device::Bus0, Device::Nvs};
    for (int i = 0; i < 4; ++i) {
      const DeviceHealth& h = health_.get(devs[i]);
      const bool bad = (h.h == Health::Fault || h.h == Health::Absent) && errInfo(h.code).sev >= Severity::Fault;
      if (bad && prevHealth_[i] == Health::Ok) ui_.showCard(h.code);
      prevHealth_[i] = bad ? Health::Fault : h.h == Health::Ok ? Health::Ok : prevHealth_[i];
    }
  }
}

// ---------------------------------------------------------------- shutdown sequencer (§7.5)
bool AppCore::startShutdown(MonoUs now, bool manual) {
  if (shut_ != Shut::Idle) return false;
  const bool timerSoon = tm_.state() == Countdown::State::Running &&
                         tm_.remainingUs(now) <= ms(kTimerWakeLeadMs + 1000);
  if (timerSoon || alertActive_) {   // too soon to arm the RTC wake: deferred, not silently downgraded
    deferredOff_ = true;
    ui_.toast(S_T_OFF_AFTER, nullptr, now);
    power_.displayOffNow(now, screenTimeoutMs());
    log_.log(LogLevel::I, "PWR", "power-off deferred until the timer alert");
    return false;
  }
  shutManual_ = manual;
  time_.checkpoint();
  p_.rtcStore->state().lastSleep = manual ? 2 : 1;
  snapshot(now);
  log_.log(LogLevel::I, "PWR", "%s power-off", manual ? "manual" : "auto");
  shutAt_ = now;
  if (manual) {
    power_.wake(now, brightness());
    ui_.openPowerOff(now);
    shut_ = Shut::Card;
  } else {
    shut_ = Shut::Quiesce;
  }
  return true;
}

bool AppCore::tickShutdown(MonoUs now, Directive& d) {
  switch (shut_) {
    case Shut::Idle: return false;
    case Shut::Card:
      if (now - shutAt_ < ms(kPowerOffCardMs)) return false;
      shut_ = Shut::Quiesce;
      shutAt_ = now;
      return false;
    case Shut::Quiesce:
      compass_.standbyForSleep();
      if (thermo_.busy() && now - shutAt_ < ms(kShtHeaterBusyMaxMs)) return false;
      shut_ = Shut::Keys;
      shutAt_ = now;
      return false;
    case Shut::Keys: {
      const uint8_t raw = p_.keys->rawPressedMask();
      shutStuck_ = input_.stuckMask();
      if ((raw & (uint8_t)~shutStuck_) && now - shutAt_ < ms(kKeyReleaseWaitMs)) return false;
      shutStuck_ |= raw;   // still LOW after 3 s: excluded from the wake mask, card at the next boot
      p_.rtcStore->state().stuckKeyMask = shutStuck_;
      if (shutStuck_ == 0x0F) {
        log_.log(LogLevel::E, "KEY", "E-KEY-STUCK all four keys low: deep sleep aborted");
        ui_.closePowerOff();
        ui_.showCard(ErrCode::KeyStuck, (uint8_t)(firstKey(shutStuck_) | (1 << 4)));
        shut_ = Shut::Idle;
        return false;
      }
      if (shutStuck_) log_.log(LogLevel::W, "KEY", "E-KEY-STUCK keys %02X low at sleep: excluded from wake", shutStuck_);
      shut_ = Shut::Ready;
      return false;
    }
    case Shut::Ready: {
      d.sleep = Directive::Sleep::Deep;
      d.deep.ext1Mask = kWakeMask & ~gpioMaskFromKeys(shutStuck_);
      d.deep.keepRtcPeriph = kDeepSleepKeepRtcPeriph;
      d.deep.timerUs = 0;
      if (tm_.state() == Countdown::State::Running) {
        const int64_t rem = tm_.remainingUs(now);
        if (rem > ms(kTimerWakeLeadMs + 1000)) d.deep.timerUs = (uint64_t)(rem - ms(kTimerWakeLeadMs));
      }
      snapshot(now);
      log_.log(LogLevel::I, "PWR", "deep sleep: ext1 %02llX timer %llu ms", (unsigned long long)d.deep.ext1Mask,
               (unsigned long long)(d.deep.timerUs / 1000));
      shut_ = Shut::Idle;   // only reached again via onDeepSleepFailed()
      return true;
    }
  }
  return false;
}

void AppCore::onDeepSleepFailed(SleepResult r) {
  const MonoUs now = p_.mono->now();
  log_.log(LogLevel::W, "PWR", "E-PWR-SLEEP deep sleep rejected (%u), staying awake", (unsigned)r);
  health_.set(Device::Power, Health::Degraded, ErrCode::SleepFail, now);
  if (p_.display) p_.display->retryInit();
  ui_.closePowerOff();
  ui_.forceFullRefresh();
  power_.wake(now, brightness());
  ui_.toast(S_T_SLEEP_FAIL, nullptr, now);
}

void AppCore::noteLightSleep(SleepResult r) { power_.noteLightSleep(r, p_.mono->now()); }

// ---------------------------------------------------------------- one loop iteration (§4.5)
Directive AppCore::step() {
  const MonoUs now = p_.mono->now();
  Directive d{Directive::Sleep::None, 0, 0, DeepSleepPlan{0, 0, false}, 0};
  handleKeys(now);
  tickModels(now);
  tickSensors(now);
  tickSerial(now);
  time_.tick(now);
  // splash: >= 600 ms, <= 1.5 s while the probes run
  if (splash_ && now - bootAt_ >= ms(kSplashMinMs) && (!thermo_.probing() || now - bootAt_ >= ms(kSplashMaxMs))) {
    splash_ = false;
    ui_.showSplash(false);
  }
  // power (§4.5 step 7)
  if (tickShutdown(now, d)) return d;
  const bool shutting = shut_ != Shut::Idle;
  PowerInputs pi{};
  pi.now = now;
  pi.keyDown = input_.downMask() != 0;
  pi.alertActive = alertActive_;
  pi.keepAwake = ui_.keepAwake() || shutting || splash_ || (ui_.state().card == ErrCode::I2cStuck);
  pi.sensorBusy = thermo_.busy() && !(thermo_.view().heaterUi == 1);
  pi.animating = ui_.animating(now);
  pi.shutdownRequested = shutting;
  pi.timerRunning = tm_.state() == Countdown::State::Running;
  pi.timerRemainingUs = tm_.remainingUs(now);
  pi.screenTimeoutMs = screenTimeoutMs();
  pi.autoOffMs = autoOffMs();
  pi.lightSleepSetting = optionValue(SettingId::LightSleep, settings_.get(SettingId::LightSleep)) != 0;
  pi.animations = ui_.state().animations;
  pi.level = brightness();
  MonoUs next = power_.nextPowerDeadline(pi);
  auto earlier = [&](MonoUs t) { if (t.v < next.v) next = t; };
  if (pi.timerRunning) earlier(tm_.deadline());
  if (alertActive_) earlier(now + ms(kAlertFlashPeriodMs));
  earlier(thermo_.nextDue(now, thermoCtx(false)));
  earlier(compass_.nextDue(now));
  if (power_.displayOn()) earlier(ui_.nextFrameAt());
  if (now.v < magStreamUntil_.v || tlmS_) earlier(now + 20 * kUsPerMs);
  pi.nextDeadline = next;
  const PowerManager::Out po = power_.step(pi);
  if (po.wentOff) { ui_.revertEdits(); log_.log(LogLevel::D, "PWR", "display off"); }
  if (po.wentOn || (power_.displayOn() && !lastDisplayOn_)) ui_.forceFullRefresh();
  if (power_.displayOn() && !lastDisplayOn_) thermo_.measureSoon();   // immediate read on display wake
  lastDisplayOn_ = power_.displayOn();
  ui_.tick(now, power_.display() == DisplayState::On || power_.display() == DisplayState::Dim);
  if (po.autoPowerOff && !shutting) startShutdown(now, false);
  // display health: lost (no ACK / 3 transfer errors) -> headless; back after a retry -> full refresh
  const bool dok = p_.display->ok();
  if (dok != displayOk_) {
    displayOk_ = dok;
    if (dok) { health_.set(Device::Oled, Health::Ok, ErrCode::None, now); ui_.forceFullRefresh(); }
    else health_.set(Device::Oled, Health::Fault, ErrCode::OledIo, now);
  }
  // frame (§4.5 step 8)
  if (power_.displayOn() && p_.display->ok()) {
    if (ui_.wantsFrame(vm_, now)) {
      buildViewModel(now);
      const uint16_t rej = ui_.rejectedLastFrame();
      ui_.render(vm_, now);
      if (ui_.rejectedLastFrame() && !rej) log_.log(LogLevel::W, "UI", "screen %u: %u draws rejected", (unsigned)ui_.topScreen(vm_), ui_.rejectedLastFrame());
    }
    const MonoUs f0 = p_.mono->now();
    ui_.flush(kFlushBudgetUs, now);
    const int64_t fu = p_.mono->now() - f0;
    if (fu > (int64_t)flushUs_) flushUs_ = (uint32_t)fu;
  } else {
    buildViewModel(now);   // keep bindings and models current while dark
  }
  log_.pump();
  const int64_t loop = p_.mono->now() - now;
  if (loop > (int64_t)loopMaxUs_) loopMaxUs_ = (uint32_t)loop;
  if (po.lightSleep && !shutting) {
    d.sleep = Directive::Sleep::Light;
    d.lightUs = po.lightUs;
    d.excludeKeys = input_.stuckMask();
    if (p_.serial) p_.serial->flushBeforeSleep();
    return d;
  }
  const int64_t idle = next.v - p_.mono->now().v;
  d.idleMs = (uint16_t)(idle <= 0 ? 0 : idle / 1000 > kMaxIdleMs ? kMaxIdleMs : idle / 1000);
  return d;
}

// ---------------------------------------------------------------- serial (§4.5 step 4)
void AppCore::tickSerial(MonoUs now) {
  if (!p_.serial) return;
  const bool host = p_.serial->hostConnected();
  if (host && !hostWasConnected_) logStartReplay(*this);   // a monitor attached after boot still sees everything
  hostWasConnected_ = host;
  char buf[64];
  const size_t n = p_.serial->read(buf, sizeof buf);
  for (size_t i = 0; i < n; ++i) {
    const char ch = buf[i];
    if (ch == '\n' || ch == '\r') {
      if (rxLen_ || rxOverflow_) {
        rx_[rxLen_] = 0;
        CmdLine c = rxOverflow_ ? CmdLine{} : parseLine(rx_);
        if (rxOverflow_) { c.cmd = Cmd::Error; c.err = "LINE too long (max 96 characters)"; }
        execute(c);
      }
      rxLen_ = 0;
      rxOverflow_ = false;
      continue;
    }
    if (rxLen_ < 96) rx_[rxLen_++] = ch;
    else rxOverflow_ = true;
  }
  if (tlmS_ && now - lastTlm_ >= (int64_t)tlmS_ * kUsPerS) { lastTlm_ = now; logTelemetry(*this); }
}

// ---------------------------------------------------------------- view model (§4.3)
void AppCore::buildViewModel(MonoUs now) {
  ViewModel& v = vm_;
  vmSettings_ = liveSettings();
  v.settings = &vmSettings_;
  const TzDef& z = kTzTable[sanitize(SettingId::TimeZone, vmSettings_.get(SettingId::TimeZone))];
  // clock
  ClockView& c = v.clock;
  c.valid = time_.valid();
  c.utcS = time_.utcS();
  c.local = utcToLocal(c.utcS, z);
  snprintf(c.zone, sizeof c.zone, "%s", tzAbbr(z, c.utcS));
  c.offsetMin = utcOffsetMin(z, c.utcS);
  c.q = time_.meta().q;
  c.src = time_.meta().src;
  c.syncAgeS = (int32_t)time_.syncAgeS();
  // thermo, mag
  v.thermo = thermo_.view();
  v.thermo.chip = thermo_.chip();
  v.thermo.addr = thermo_.addr();
  v.mag = compass_.view();
  v.mag.drdyPerS = drdyRate_;
  // stopwatch
  StopwatchView& s = v.sw;
  s.st = sw_.state();
  s.elapsedUs = sw_.elapsedUs(now);
  s.curLapUs = sw_.currentLapUs(now);
  s.lapNo = sw_.lapNo();
  s.laps = sw_.storedLaps();
  s.lastLapUs = sw_.lapUs(0);
  s.lastDeltaUs = sw_.lapDeltaUs(0);
  // timer
  TimerView& t = v.tm;
  t.st = tm_.state();
  t.preset = tm_.preset();
  t.remainingUs = tm_.remainingUs(now);
  t.totalUs = tm_.totalUs();
  t.overtimeUs = tm_.overtimeUs(now);
  t.endsKnown = c.valid && t.st != Countdown::State::Paused;
  int64_t endsUtc = c.utcS;
  if (t.st == Countdown::State::Running || t.st == Countdown::State::Select) endsUtc += Countdown::displaySeconds(t.remainingUs);
  else if (t.st == Countdown::State::Done) endsUtc -= t.overtimeUs / kUsPerS;
  t.endsLocal = utcToLocal(endsUtc, z);
  t.alertActive = alertActive_;
  t.alertPhase = alertActive_ && now - alertAt_ >= ms(kAlertFlashMs) ? 1 : 0;
  t.missed = missedBell_;
  // target
  v.target.set = compass_.targetSet();
  v.target.bearing = compass_.target();
  v.target.steerDeg = wrap180((float)compass_.target() - v.mag.headingDeg);
  v.bat = bat_;
  v.cal = compass_.calView(now);
  // system
  SystemView& y = v.sys;
  y.fw = kFwVersion;
  y.build = bootIn_.buildDate;
  y.coreVer = bootIn_.coreVersion;
  y.resetReason = wake_.resetReason;
  y.wakeCause = wake_.cause;
  y.wakeKey = firstKey(keysFromGpioMask(wake_.ext1Mask));
  y.wakeSampleUs = wake_.sampleUs;
  y.uptimeS = (now - bootAt_) / kUsPerS;
  y.heapFree = p_.power ? p_.power->heapFree() : 0;
  y.usbHost = power_.usbHost();
  y.cdcConnected = p_.serial && p_.serial->hostConnected();
  y.loopMaxUs = loopMaxUs_;
  y.flushUs = flushUs_;
  if (p_.sensorBus) y.bus0 = p_.sensorBus->stats();
  if (p_.oledBus) y.bus1 = p_.oledBus->stats();
  memcpy(y.scan0, scan0_, sizeof y.scan0); y.n0 = n0_;
  memcpy(y.scan1, scan1_, sizeof y.scan1); y.n1 = n1_;
  memcpy(y.keyPresses, keyPresses_, sizeof y.keyPresses);
  y.keyDownMask = input_.downMask();
  y.keyStuckMask = input_.stuckMask();
  y.serialDropped = log_.droppedBytes();
  y.busErr = health_.get(Device::Bus0).code;
  y.nvsErr = health_.get(Device::Nvs).code;
  y.heatValid = thermo_.heatValid();
  y.heatPreC10 = (uint16_t)(int16_t)(thermo_.heatPre() * 10);
  y.heatHotC10 = (uint16_t)(int16_t)(thermo_.heatHot() * 10);
  // chips
  BgChips& b = v.bg;
  b.warn = health_.anyWarnOrWorse();
  b.sw = s.st == Stopwatch::State::Running;
  b.tmr = t.st == Countdown::State::Running;
  b.tmrRemainS = (int32_t)Countdown::displaySeconds(t.remainingUs);
  b.bell = missedBell_;
  b.syncTag = c.valid && c.q == ClockQuality::Approx;
  // fmt context
  FormatCtx& f = v.fmt;
  f.sht = thermo_.chip();
  f.mag = compass_.chip();
  f.liveHeadingDeg = v.mag.headingDeg;
  f.liveTempC = thermo_.lastRawT();
  f.utcS = c.utcS;
  f.haveHeading = v.mag.valid;
  f.haveTemp = v.thermo.valid;
  f.calPct = compass_.calibration().valid ? (int8_t)compass_.calibration().qualityPct : (int8_t)-1;
  f.calGrade = compass_.calibration().qualityPct >= 92 ? 0 : 1;
}

}  // namespace hg
