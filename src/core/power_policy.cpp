#include "power_policy.h"

#include "../config/timing.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }
}  // namespace

PowerOut decidePower(const PowerIn& in) {
  PowerOut out{DisplayState::On, false, 0, false};
  const int64_t idle = in.now - in.lastActivity;
  if (in.alertActive || in.keepAwake || in.keyDown || in.screenTimeoutMs == 0) {
    out.display = DisplayState::On;              // a held (non-stuck) key suspends the timeout (§2.3.3)
  } else {
    const uint32_t dimLead = in.screenTimeoutMs <= 10000 ? kDimLeadShortMs : kDimLeadMs;
    if (idle >= ms(in.screenTimeoutMs)) out.display = DisplayState::Off;
    else if (idle >= ms(in.screenTimeoutMs) - ms(dimLead)) out.display = DisplayState::Dim;
  }
  const int64_t toDeadline = in.nextDeadline - in.now;
  out.lightSleep = out.display == DisplayState::Off && in.lightSleepSetting && !in.usbHost && !in.usbRaw &&
                   !in.usbPending && in.now >= in.usbProbeUntil && !in.keyDown && !in.sensorBusy && !in.animating &&
                   !in.shutdownRequested && toDeadline >= ms(kLightSleepMinMs);
  if (out.lightSleep) {
    const int64_t us = toDeadline < ms(kLightSleepMaxSliceMs) ? toDeadline : ms(kLightSleepMaxSliceMs);
    out.lightSleepUs = (uint32_t)us;
  }
  out.autoPowerOff = out.display == DisplayState::Off && in.autoOffMs != 0 &&
                     in.now.v >= in.displayOffSince.v + ms(in.autoOffMs) && !in.usbHost && !in.alertActive &&
                     !in.keepAwake && !in.shutdownRequested &&
                     !(in.timerRunning && in.timerRemainingUs < ms(kAutoOffTimerVetoMs));
  return out;
}

void UsbDebouncer::feed(bool raw, MonoUs now) {
  if (raw != raw_) {
    raw_ = raw;
    since_ = now;
  }
  if (raw_ != plugged_ && now - since_ >= ms(raw_ ? kUsbPlugOnMs : kUsbPlugOffMs)) plugged_ = raw_;
}

bool UsbDebouncer::plugged() const { return plugged_; }
bool UsbDebouncer::pending() const { return raw_ != plugged_; }

}  // namespace hg
