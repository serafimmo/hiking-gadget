#include "power_manager.h"

#include "../config/timing.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }
uint8_t contrastOf(Brightness b) { return b == Brightness::High ? 0xCF : b == Brightness::Low ? 0x01 : 0x40; }
}  // namespace

void PowerManager::applyLevel(Brightness b) {
  if (display_) display_->setLevel(b);
}

void PowerManager::begin(MonoUs now, bool displayOn, Brightness level) {
  lastActivity_ = now;
  level_ = level;
  state_ = displayOn ? DisplayState::On : DisplayState::Off;
  if (!displayOn) offSince_ = now;
  if (display_) {
    display_->setPower(displayOn);
    if (displayOn) applyLevel(level);
  }
}

void PowerManager::wake(MonoUs now, Brightness level) {
  lastActivity_ = now;
  level_ = level;
  if (state_ == DisplayState::Off && display_) {
    display_->setPower(true);
    display_->setContrastRaw(0);
    fade_ = 1;
    fadeAt_ = now;
  } else {
    applyLevel(level);
    fade_ = 0;
  }
  state_ = DisplayState::On;
}

void PowerManager::displayOffNow(MonoUs now, uint32_t screenTimeoutMs) {
  lastActivity_ = MonoUs{now.v - (int64_t)(screenTimeoutMs ? screenTimeoutMs : 3600000u) * kUsPerMs - 1};
}

void PowerManager::noteLightSleep(SleepResult r, MonoUs now) {
  if (r == SleepResult::Ok) {
    ++slices_;
    // USB probe window (D28): after every 8th light-sleep wake stay awake 300 ms so a newly plugged host is seen
    if (slices_ % kUsbProbeEverySlices == 0) probeUntil_ = now + ms(kUsbProbeAwakeMs);
  } else {
    ++rejected_;
  }
}

PowerManager::Out PowerManager::step(const PowerInputs& in) {
  Out o{state_, false, 0, false, false, false};
  if (power_) usbRaw_ = power_->usbHostRaw();
  usb_.feed(usbRaw_, in.now);
  PowerIn pin{};
  pin.now = in.now;
  pin.lastActivity = lastActivity_;
  pin.displayOffSince = offSince_;
  pin.screenTimeoutMs = in.screenTimeoutMs;
  pin.autoOffMs = in.autoOffMs;
  pin.lightSleepSetting = in.lightSleepSetting;
  pin.usbHost = usb_.plugged();
  pin.keyDown = in.keyDown;
  pin.alertActive = in.alertActive;
  pin.keepAwake = in.keepAwake;
  pin.sensorBusy = in.sensorBusy;
  pin.animating = in.animating || fade_ != 0;
  pin.shutdownRequested = in.shutdownRequested;
  pin.usbRaw = usbRaw_;
  pin.usbPending = usb_.pending();
  pin.usbProbeUntil = probeUntil_;
  pin.timerRunning = in.timerRunning;
  pin.timerRemainingUs = in.timerRemainingUs;
  pin.nextDeadline = in.nextDeadline;
  const PowerOut d = decidePower(pin);
  const Brightness lvl = forceHigh_ ? Brightness::High : in.level;
  level_ = in.level;
  // display transitions
  if (d.display != state_) {
    if (d.display == DisplayState::Off) {
      if (in.animations && fade_ != 2) { fade_ = 2; fadeAt_ = in.now; }
      if (!in.animations || in.now - fadeAt_ >= ms(kFadeOutMs)) {
        if (display_) display_->setPower(false);
        state_ = DisplayState::Off;
        offSince_ = in.now;
        fade_ = 0;
        o.wentOff = true;
      }
    } else if (d.display == DisplayState::Dim) {
      applyLevel(Brightness::Low);
      state_ = DisplayState::Dim;
    } else {
      if (state_ == DisplayState::Off && display_) { display_->setPower(true); o.wentOn = true; }
      applyLevel(lvl);
      state_ = DisplayState::On;
      fade_ = 0;
    }
  } else if (state_ == DisplayState::On) {
    if (fade_ == 1) {   // contrast ramp 0 -> level in 120 ms, 6 steps (§3.6 motion 5)
      const int64_t t = in.now - fadeAt_;
      if (!in.animations || t >= ms(kFadeInMs)) { applyLevel(lvl); fade_ = 0; }
      else if (display_) display_->setContrastRaw((uint8_t)(contrastOf(lvl) * (1 + t * 6 / ms(kFadeInMs)) / 6));
    } else if (fade_ == 2) {   // the off decision was withdrawn mid-fade (a key)
      applyLevel(lvl);
      fade_ = 0;
    }
  }
  if (fade_ == 2 && display_) {
    const int64_t t = in.now - fadeAt_;
    const int64_t left = ms(kFadeOutMs) - t;
    display_->setContrastRaw((uint8_t)(left > 0 ? contrastOf(level_) * left / ms(kFadeOutMs) : 0));
  }
  o.display = state_;
  o.lightSleep = d.lightSleep && state_ == DisplayState::Off;
  o.lightUs = d.lightSleepUs;
  o.autoPowerOff = d.autoPowerOff && state_ == DisplayState::Off;
  return o;
}

MonoUs PowerManager::nextPowerDeadline(const PowerInputs& in) const {
  MonoUs t{INT64_MAX};
  auto earlier = [&](MonoUs v) { if (v.v > in.now.v && v.v < t.v) t = v; };
  if (in.screenTimeoutMs && state_ != DisplayState::Off) {
    const uint32_t lead = in.screenTimeoutMs <= 10000 ? kDimLeadShortMs : kDimLeadMs;
    earlier(lastActivity_ + ms(in.screenTimeoutMs - lead));
    earlier(lastActivity_ + ms(in.screenTimeoutMs));
  }
  if (state_ == DisplayState::Off && in.autoOffMs) earlier(offSince_ + ms(in.autoOffMs));
  if (usbRaw_ || usb_.pending()) earlier(in.now + ms(kUsbPlugOnMs));
  if (probeUntil_.v > in.now.v) earlier(probeUntil_);
  return t;
}

}  // namespace hg
