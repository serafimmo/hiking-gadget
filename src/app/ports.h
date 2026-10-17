#pragma once
// Hardware ports: implemented by hal/ on the device and by tools/host/fakes on the PC.
#include "../core/rtc_bridge.h"      // RtcState
#include "../drivers/i2c_bus.h"      // II2cBus
#include "../ui/display_port.h"      // IDisplay is NOT redefined here
namespace hg {
struct IMonoClock { virtual MonoUs now() const = 0; };
struct IRtcClock  { virtual RtcUs  now() const = 0; };
struct IWallClock { virtual bool get(UtcMs* out) const = 0; virtual bool set(UtcMs t) = 0; };
struct IKeys      { virtual uint8_t rawPressedMask() = 0; };      // logical key bits, polarity applied
struct IKvStore { virtual bool ok() const = 0;
  virtual bool get(const char* key, void* buf, size_t len) = 0;  // true only if the key exists with exactly len bytes
  virtual bool put(const char* key, const void* buf, size_t len) = 0;  virtual bool erase(const char* key) = 0;
  virtual bool eraseAll() = 0; };
struct IRtcStore  { virtual RtcState& state() = 0; };             // RTC_NOINIT object
struct WakeInfo   { ResetReason resetReason; WakeCause cause; uint64_t ext1Mask; uint8_t levelsAtBoot /*logical keys LOW*/;
                     uint32_t sampleUs; };                         // esp_timer time of the boot key sample (§4.6 step 2)
struct DeepSleepPlan { uint64_t ext1Mask; uint64_t timerUs; bool keepRtcPeriph; };
enum class SleepResult : uint8_t { Ok, Rejected, TooShort, Error };
struct IPower {
  virtual WakeInfo wakeInfo() const = 0;  virtual bool usbHostRaw() = 0;         // HWCDC::isPlugged()
  virtual SleepResult lightSleep(uint32_t us, uint8_t excludeKeys) = 0;          // keys + timer wake; stuck keys excluded; §7.4
  virtual SleepResult deepSleep(const DeepSleepPlan& p) = 0;                     // returns only on failure; §7.5
  virtual void restart() = 0;  virtual uint32_t heapFree() const = 0;
};
struct ISerialPort { virtual size_t read(char* buf, size_t max) = 0;
                     virtual size_t write(const char* s, size_t n) = 0;          // bytes accepted; short when the TX ring is full
                     virtual size_t writable() = 0;                              // HWCDC::availableForWrite()
                     virtual bool hostConnected() = 0; virtual void flushBeforeSleep() = 0; };
struct IBattery    { virtual BatteryState read() = 0; };                        // core type: hal never depends on ui
struct IDrdy       { virtual void enable(DrdyKind k) = 0; virtual void disable() = 0; virtual uint32_t edges() const = 0; };
struct Ports { IMonoClock* mono; IRtcClock* rtc; IWallClock* wall; IKeys* keys; IDisplay* display; IKvStore* kv;
               IRtcStore* rtcStore; IPower* power; ISerialPort* serial; IBattery* battery; IDrdy* drdy;
               II2cBus* sensorBus; II2cBus* oledBus; };
}  // namespace hg
