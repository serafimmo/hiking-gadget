#pragma once
#include "types.h"
namespace hg {
enum class DisplayState : uint8_t { On, Dim, Off };
struct PowerIn {
  MonoUs now, lastActivity, displayOffSince;
  uint32_t screenTimeoutMs;  uint32_t autoOffMs;            // 0 = never
  bool lightSleepSetting, usbHost, keyDown, alertActive, keepAwake, sensorBusy, animating, shutdownRequested;
  // keyDown: any NON-STUCK key down (InputLogic::downMask() != 0); a stuck key never counts (§2.2)
  bool usbRaw, usbPending;                                  // HWCDC::isPlugged() now; UsbDebouncer::pending()
  MonoUs usbProbeUntil;                                     // stay awake until then to sample USB (every 8th light-sleep wake, §7.2)
  bool timerRunning; int64_t timerRemainingUs;
  MonoUs nextDeadline;                                      // earliest of all scheduled work
};
struct PowerOut { DisplayState display; bool lightSleep; uint32_t lightSleepUs; bool autoPowerOff; };
PowerOut decidePower(const PowerIn& in);                    // rules §7.2
class UsbDebouncer { public: void feed(bool raw, MonoUs now); bool plugged() const;
                     bool pending() const;                  // on after 1 s true, off after 3 s false; starts plugged;
                                                            // pending = raw != plugged() (a debounce is counting)
                     private: bool plugged_ = true, raw_ = true; MonoUs since_{0}; };
}  // namespace hg
