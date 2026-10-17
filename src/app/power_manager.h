#pragma once
// Display states with fades, light-sleep directive, USB debounce and probe window, auto power-off (SPEC §7.1-7.3).
#include "../core/power_policy.h"
#include "logger.h"
#include "ports.h"

namespace hg {
struct PowerInputs {
  MonoUs now;
  bool keyDown, alertActive, keepAwake, sensorBusy, animating, shutdownRequested, timerRunning;
  int64_t timerRemainingUs;
  MonoUs nextDeadline;
  uint32_t screenTimeoutMs, autoOffMs;
  bool lightSleepSetting, animations;
  Brightness level;
};

class PowerManager {
 public:
  PowerManager(IPower* power, IDisplay* display, Logger& log) : power_(power), display_(display), log_(log) {}
  void begin(MonoUs now, bool displayOn, Brightness level);
  void activity(MonoUs now) { lastActivity_ = now; }
  void wake(MonoUs now, Brightness level);                   // a key while dark: on, fade in
  void displayOffNow(MonoUs now, uint32_t screenTimeoutMs);  // * on Home
  void forceHigh(bool on) { forceHigh_ = on; }               // alert phase A/B
  struct Out { DisplayState display; bool lightSleep; uint32_t lightUs; bool autoPowerOff; bool wentOff, wentOn; };
  Out step(const PowerInputs& in);
  void noteLightSleep(SleepResult r, MonoUs now);
  bool displayOn() const { return state_ != DisplayState::Off; }
  DisplayState display() const { return state_; }
  bool usbHost() const { return usb_.plugged(); }
  bool usbRaw() const { return usbRaw_; }
  MonoUs displayOffSince() const { return offSince_; }
  MonoUs lastActivity() const { return lastActivity_; }
  uint32_t lightSleeps() const { return slices_; }
  uint32_t rejectedSleeps() const { return rejected_; }
  MonoUs nextPowerDeadline(const PowerInputs& in) const;   // DIM/OFF moments, auto-off, USB re-check, probe end

 private:
  void applyLevel(Brightness b);
  IPower* power_;
  IDisplay* display_;
  Logger& log_;
  UsbDebouncer usb_;
  bool usbRaw_ = true;
  DisplayState state_ = DisplayState::On;
  MonoUs lastActivity_{0}, offSince_{0}, probeUntil_{0}, fadeAt_{0};
  uint8_t fade_ = 0;                                          // 0 none, 1 fading in, 2 fading out
  Brightness level_ = Brightness::Mid;
  bool forceHigh_ = false;
  uint32_t slices_ = 0, rejected_ = 0;
};
}  // namespace hg
