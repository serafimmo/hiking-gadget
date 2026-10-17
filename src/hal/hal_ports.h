#pragma once
// ESP32 implementations of the app ports (SPEC §4.4 app/ports.h). Device only.
#include <Preferences.h>
#include <Wire.h>

#include "../app/ports.h"
#include "../ui/u8g2_inc.h"

namespace hg {
// ---- clocks (hal_clock.cpp: the only file allowed to set the system time)
struct HalMonoClock : IMonoClock { MonoUs now() const override; };
struct HalRtcClock : IRtcClock { RtcUs now() const override; };
struct HalWallClock : IWallClock { bool get(UtcMs* out) const override; bool set(UtcMs t) override; };
// ---- keys (hal_keys.cpp)
struct HalKeys : IKeys {
  void begin();                      // INPUT_PULLUP on the four key pins (after the boot wake sample)
  uint8_t rawPressedMask() override; // logical bits, LOW = pressed
};
// Key pins back to digital inputs with pull-ups: at boot (held / RTC-routed by the previous deep sleep) and after a
// rejected deep sleep (the ext1 setup routes them to RTC before the reject, §7.5).
void releaseKeyPins();
// ---- I2C (hal_i2c.cpp)
class HalI2c : public II2cBus {
 public:
  HalI2c(TwoWire& w, uint8_t sda, uint8_t scl, uint32_t hz) : w_(w), sda_(sda), scl_(scl), hz_(hz) {}
  bool begin();                      // stuck-line check (+ recovery), then begin + timeout; false = SDA stuck
  bool linesHigh();
  void end() { w_.end(); }
  bool probe(uint8_t addr) override;
  I2cStatus write(uint8_t addr, const uint8_t* d, size_t n) override;
  I2cStatus read(uint8_t addr, uint8_t* d, size_t n) override;
  I2cStatus writeRead(uint8_t addr, const uint8_t* w, size_t wn, uint8_t* r, size_t rn) override;
  bool recover() override;
  uint8_t scan(uint8_t* found, uint8_t max) override;
  const I2cStats& stats() const override { return stats_; }
  bool stuck() const { return stuck_; }
 private:
  I2cStatus count(I2cStatus s);
  bool down();                       // stuck: true until a line check (every 30 s) passes
  bool pulse();                      // 9 SCL clocks + STOP, then the line check
  TwoWire& w_;
  uint8_t sda_, scl_;
  uint32_t hz_;
  I2cStats stats_{};
  bool stuck_ = false;
  uint32_t recWindowStart_ = 0, lastCheckMs_ = 0;
  uint8_t recInWindow_ = 0;
};
// ---- display (hal_oled.cpp)
class HalOled : public IDisplay {
 public:
  bool begin(bool fastInit, Brightness level);
  bool ok() const override;
  u8g2_t* u8g2() override { return &g_; }
  void sendPageSpan(uint8_t page, uint8_t tx0, uint8_t tw) override;
  void setPower(bool on) override;
  void setLevel(Brightness b) override;
  void setContrastRaw(uint8_t v) override;
  void setInvert(bool on) override;
  void deepOff() override;
  bool retryInit() override;
  uint32_t ioErrors() const override;
  uint8_t address() const { return addr_; }
 private:
  void cmds(const uint8_t* c, uint8_t n);
  u8g2_t g_{};
  uint8_t addr_ = 0;
  bool present_ = false, needInit_ = false;
  bool on_ = false;                  // last setPower() request: the PowerManager owns on/off, a re-init restores it
  Brightness level_ = Brightness::Mid;
  uint32_t lastRetryMs_ = 0;
};
// ---- storage (hal_store.cpp)
class HalKv : public IKvStore {
 public:
  bool begin();                      // Preferences namespace "hg2"
  bool ok() const override { return ok_; }
  bool get(const char* key, void* buf, size_t len) override;
  bool put(const char* key, const void* buf, size_t len) override;
  bool erase(const char* key) override;
  bool eraseAll() override;
 private:
  Preferences p_;
  bool ok_ = false;
};
struct HalRtcStore : IRtcStore { RtcState& state() override; };
// ---- serial (hal_serial.cpp)
struct HalSerial : ISerialPort {
  void begin();
  size_t read(char* buf, size_t max) override;
  size_t write(const char* s, size_t n) override;
  size_t writable() override;
  bool hostConnected() override;
  void flushBeforeSleep() override;
};
// ---- power (hal_power.cpp)
class HalPower : public IPower {
 public:
  void captureWake(uint8_t levelsAtBoot, uint32_t sampleUs);   // §4.6 step 2
  void attach(HalOled* d, HalI2c* b0, HalI2c* b1, HalSerial* s) { oled_ = d; bus0_ = b0; bus1_ = b1; serial_ = s; }
  WakeInfo wakeInfo() const override { return info_; }
  bool usbHostRaw() override;
  SleepResult lightSleep(uint32_t us, uint8_t excludeKeys) override;
  SleepResult deepSleep(const DeepSleepPlan& p) override;
  void restart() override;
  uint32_t heapFree() const override;
 private:
  WakeInfo info_{};
  HalOled* oled_ = nullptr;
  HalI2c *bus0_ = nullptr, *bus1_ = nullptr;
  HalSerial* serial_ = nullptr;
};
// ---- battery, DRDY (hal_battery.cpp, hal_drdy.cpp)
struct HalBattery : IBattery { BatteryState read() override; };
struct HalDrdy : IDrdy { void enable(DrdyKind k) override; void disable() override; uint32_t edges() const override; };
}  // namespace hg
