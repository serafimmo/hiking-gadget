#pragma once
// A whole gadget on the PC: every port faked, AppCore stepped like hal/board does, light and deep sleep simulated.
#include <memory>
#include <string>

#include "app/app_core.h"
#include "fakes/fake_display.h"
#include "fakes/fake_i2c.h"
#include "fakes/fake_mag.h"
#include "fakes/fake_ports.h"
#include "fakes/fake_sht.h"
#include "fakes/host_u8g2.h"

namespace hg {
struct World {
  WorldClock clk;
  FakeMono mono{&clk};
  FakeRtcClock rtc{&clk};
  FakeWall wall{&clk};
  FakeKeys keys;
  HostU8g2 oled;
  FakeDisplay display{&oled.g};
  FakeKv kv;
  FakeRtcStore rtcStore;
  FakePower power;
  FakeSerial serial;
  FakeBattery battery;
  FakeDrdy drdy;
  FakeI2c bus0, bus1;
  FakeSht sht{FakeSht::Kind::Sht4x};
  FakeMag mag{FakeMag::Kind::QmcL};
  FakeRegDevice oledDev;
  std::unique_ptr<AppCore> app;
  const char* buildDate = "Oct 11 2026";
  const char* buildTime = "19:30:12";
  uint32_t boots = 0, deepSleeps = 0;
  bool asleep = false;
  int64_t deepWakeAt = -1;                 // world time of a scheduled RTC timer wake

  World() { bus0.attach(0x44, &sht); bus0.attach(0x0D, &mag); bus1.attach(0x3C, &oledDev); power.clk = &clk; }
  Ports ports() {
    return Ports{&mono, &rtc, &wall, &keys, &display, &kv, &rtcStore, &power, &serial, &battery, &drdy, &bus0, &bus1};
  }
  void bootNow() {
    clk.bootAt = clk.us;
    app.reset(new AppCore(ports()));
    app->boot(BootInputs{buildDate, buildTime, "host"});
    ++boots;
  }
  void powerOn() {           // cold boot: RTC memory garbage, wall clock lost
    memset(&rtcStore.s, 0x5A, sizeof rtcStore.s);
    clk.wallValid = false;
    power.info = WakeInfo{ResetReason::PowerOn, WakeCause::None, 0, keys.mask, 1000};
    bootNow();
  }
  int64_t ms() const { return clk.us / 1000; }
  void syncBus() { bus0.setNow(MonoUs{clk.us}); bus1.setNow(MonoUs{clk.us}); }   // devices keep their own time across resets
  // One loop iteration (hal/board.cpp boardLoop). Returns the directive.
  Directive step() {
    syncBus();
    const Directive d = app->step();
    serial.drain();
    switch (d.sleep) {
      case Directive::Sleep::Light: {
        // a key that is down ends light sleep at once; else the timer slice elapses
        if (keys.mask & ~d.excludeKeys) { app->noteLightSleep(SleepResult::Rejected); clk.us += 1000; break; }
        clk.us += d.lightUs;
        power.lastWakeUs = clk.us;   // light sleep gates the USB pads: the host is seen again ~150 ms after waking
        app->noteLightSleep(power.lightSleep(d.lightUs, d.excludeKeys));
        break;
      }
      case Directive::Sleep::Deep: {
        const SleepResult r = power.deepSleep(d.deep);
        if (r != SleepResult::Ok) { app->onDeepSleepFailed(r); clk.us += 1000; break; }
        ++deepSleeps;
        asleep = true;
        deepWakeAt = d.deep.timerUs ? clk.us + (int64_t)d.deep.timerUs : -1;
        break;
      }
      default:
        clk.us += (d.idleMs ? d.idleMs : 1) * 1000;
        break;
    }
    return d;
  }
  // Deep sleep: time passes until a key (in the ext1 mask) or the RTC timer wakes the chip -> boot.
  void sleepUntil(int64_t untilMs, uint8_t pressAtEnd = 0) {
    const DeepSleepPlan p = power.lastPlan;
    const int64_t until = untilMs * 1000;
    if (deepWakeAt >= 0 && deepWakeAt <= until) {
      clk.us = deepWakeAt;
      power.info = WakeInfo{ResetReason::DeepSleep, WakeCause::Timer, 0, keys.mask, 300000};
    } else {
      clk.us = until;
      uint64_t woke = 0;
      for (uint8_t k = 0; k < 4; ++k) if (pressAtEnd & (1u << k)) woke |= 1ULL << kKeyPinsForWorld[k];
      woke &= p.ext1Mask;
      power.info = WakeInfo{ResetReason::DeepSleep, woke ? WakeCause::Ext1 : WakeCause::Other, woke, keys.mask, 250000};
    }
    asleep = false;
    bootNow();
  }
  // Runs steps until 'ms' of world time passed (stops early if the device went into deep sleep).
  void run(int64_t forMs) {
    const int64_t end = clk.us + forMs * 1000;
    while (clk.us < end && !asleep) step();
  }
  void press(uint8_t mask, int64_t holdMs = 60) { keys.mask = mask; run(holdMs); keys.mask = 0; run(200); }
  void sendLine(const char* l) { serial.in += l; serial.in += "\n"; }
  bool out(const char* needle) const { return serial.out.find(needle) != std::string::npos; }
  static constexpr uint8_t kKeyPinsForWorld[4] = {4, 5, 6, 1};
};
}  // namespace hg
