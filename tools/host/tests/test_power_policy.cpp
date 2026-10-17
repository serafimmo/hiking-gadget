// core/power_policy: display timeline, light-sleep matrix, auto-off vetoes, UsbDebouncer (§7.2, §7.3).
#include "core/power_policy.h"
#include "tiny_test.h"

using namespace hg;

namespace {
MonoUs at(int64_t ms) { return MonoUs{ms * 1000}; }
PowerIn idleDark(int64_t nowMs) {          // display long off, nothing pending: light sleep allowed
  PowerIn in{};
  in.now = at(nowMs);
  in.lastActivity = at(0);
  in.displayOffSince = at(30000);
  in.screenTimeoutMs = 30000;
  in.autoOffMs = 30 * 60000;
  in.lightSleepSetting = true;
  in.usbProbeUntil = at(0);
  in.nextDeadline = at(nowMs + 10000);
  return in;
}
}  // namespace

TEST(power_display_timeline) {
  PowerIn in = idleDark(0);
  const struct { uint32_t timeout; int64_t idle; DisplayState want; } rows[] = {
      {30000, 24999, DisplayState::On}, {30000, 25000, DisplayState::Dim}, {30000, 29999, DisplayState::Dim},
      {30000, 30000, DisplayState::Off}, {10000, 6999, DisplayState::On}, {10000, 7000, DisplayState::Dim},
      {10000, 10000, DisplayState::Off}, {300000, 295000, DisplayState::Dim}, {0, 99999999, DisplayState::On}};
  for (const auto& r : rows) {
    in.screenTimeoutMs = r.timeout;
    in.lastActivity = at(1000);
    in.now = at(1000 + r.idle);
    in.nextDeadline = in.now + 10000000;
    CHECK_EQ(decidePower(in).display, r.want);
  }
  in.screenTimeoutMs = 30000; in.now = at(100000);
  in.keyDown = true;     CHECK_EQ(decidePower(in).display, DisplayState::On);   // held key suspends the timeout
  in.keyDown = false; in.alertActive = true; CHECK_EQ(decidePower(in).display, DisplayState::On);
  in.alertActive = false; in.keepAwake = true; CHECK_EQ(decidePower(in).display, DisplayState::On);
}

TEST(power_light_sleep_matrix) {
  const PowerIn base = idleDark(100000);
  PowerOut o = decidePower(base);
  CHECK(o.lightSleep);
  CHECK_EQ(o.lightSleepUs, 4000000u);                       // 4 s slice cap
  PowerIn in = base; in.nextDeadline = at(100000 + 1200);
  CHECK_EQ(decidePower(in).lightSleepUs, 1200000u);
  in = base; in.nextDeadline = at(100000 + 49); CHECK(!decidePower(in).lightSleep);   // < 50 ms
  in = base; in.nextDeadline = at(100000 + 50); CHECK(decidePower(in).lightSleep);
  in = base; in.lightSleepSetting = false; CHECK(!decidePower(in).lightSleep);
  in = base; in.usbHost = true;            CHECK(!decidePower(in).lightSleep);
  in = base; in.usbRaw = true;             CHECK(!decidePower(in).lightSleep);
  in = base; in.usbPending = true;         CHECK(!decidePower(in).lightSleep);
  in = base; in.usbProbeUntil = at(100001); CHECK(!decidePower(in).lightSleep);  // USB probe window
  in = base; in.keyDown = true;            CHECK(!decidePower(in).lightSleep);
  in = base; in.sensorBusy = true;         CHECK(!decidePower(in).lightSleep);
  in = base; in.animating = true;          CHECK(!decidePower(in).lightSleep);
  in = base; in.now = at(26000); in.nextDeadline = at(30000);
  CHECK(!decidePower(in).lightSleep);                       // display Dim: no light sleep
  // a stuck key is reported as keyDown = false by the caller: it blocks neither the timeout nor light sleep
  in = base; in.keyDown = false;           CHECK(decidePower(in).lightSleep);
}

TEST(power_auto_off_and_vetoes) {
  PowerIn in = idleDark(30000 + 30 * 60000);
  CHECK(decidePower(in).autoPowerOff);
  in.now = at(30000 + 30 * 60000 - 1);    CHECK(!decidePower(in).autoPowerOff);
  in = idleDark(30000 + 30 * 60000);
  in.autoOffMs = 0;                        CHECK(!decidePower(in).autoPowerOff);     // Never
  in = idleDark(30000 + 30 * 60000); in.usbHost = true;     CHECK(!decidePower(in).autoPowerOff);
  in = idleDark(30000 + 30 * 60000); in.alertActive = true; CHECK(!decidePower(in).autoPowerOff);
  in = idleDark(30000 + 30 * 60000); in.keepAwake = true;   CHECK(!decidePower(in).autoPowerOff);
  in = idleDark(30000 + 30 * 60000); in.timerRunning = true; in.timerRemainingUs = 59 * kUsPerS;
  CHECK(!decidePower(in).autoPowerOff);                     // timer ends within 60 s
  in.timerRemainingUs = 61 * kUsPerS;     CHECK(decidePower(in).autoPowerOff);
}

TEST(power_usb_debouncer) {
  UsbDebouncer u;
  CHECK(u.plugged()); CHECK(!u.pending());                  // starts plugged
  u.feed(false, at(0));      CHECK(u.plugged()); CHECK(u.pending());
  u.feed(false, at(2999));   CHECK(u.plugged());
  u.feed(false, at(3000));   CHECK(!u.plugged()); CHECK(!u.pending());
  u.feed(true, at(4000));    CHECK(!u.plugged()); CHECK(u.pending());
  u.feed(false, at(4500));   CHECK(!u.pending());           // a flap does not count
  u.feed(true, at(5000));
  u.feed(true, at(5999));    CHECK(!u.plugged());
  u.feed(true, at(6000));    CHECK(u.plugged());
  // flapping every 2 s while plugged never unplugs
  for (int i = 0; i < 10; ++i) { u.feed(false, at(7000 + i * 4000)); u.feed(true, at(9000 + i * 4000)); }
  CHECK(u.plugged());
}
