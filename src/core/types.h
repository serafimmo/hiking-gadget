#pragma once
// Shared value types of every layer (SPEC §4.4). Host-compilable: no Arduino/IDF headers.
#include <stddef.h>
#include <stdint.h>
namespace hg {
// Strong time types: no implicit conversion between them, ever.
struct MonoUs { int64_t v; };   // esp_timer_get_time(): XTAL while awake, compensated across light sleep, ~0 after any reset
struct RtcUs  { uint64_t v; };  // esp_rtc_get_time_us(): RTC counter (internal RC), continues across deep sleep and SW resets
struct UtcMs  { int64_t v; };   // wall clock, Unix epoch ms; display and logs only
constexpr int64_t kUsPerMs = 1000, kUsPerS = 1000000;
constexpr int64_t operator-(MonoUs a, MonoUs b) { return a.v - b.v; }            // µs
constexpr MonoUs  operator+(MonoUs a, int64_t us) { return MonoUs{a.v + us}; }
constexpr bool operator<(MonoUs a, MonoUs b)  { return a.v < b.v; }
constexpr bool operator<=(MonoUs a, MonoUs b) { return a.v <= b.v; }
constexpr bool operator>=(MonoUs a, MonoUs b) { return a.v >= b.v; }
constexpr bool operator==(MonoUs a, MonoUs b) { return a.v == b.v; }
constexpr int64_t operator-(RtcUs a, RtcUs b) { return (int64_t)(a.v - b.v); }   // µs
enum class Key : uint8_t { Up = 0, Down = 1, Hash = 2, Star = 3 };
constexpr uint8_t kKeyCount = 4;
constexpr uint8_t keyBit(Key k) { return (uint8_t)(1u << (uint8_t)k); }
enum class Health : uint8_t { Unknown, Ok, Degraded, Fault, Absent, Unsupported };
enum class Severity : uint8_t { Info, Warn, Fault, Fatal };
enum class Brightness : uint8_t { Low, Mid, High };
enum class ErrCode : uint8_t {           // table with mnemonic/severity/device in core/health.cpp, §9.4; texts in ui errTitle()/errHint()
  None, OledNone, OledIo, ShtNone, ShtUnknown, ShtCrc, ShtIo, Sht3xInfo, HeatSkipHot,
  MagNone, MagUnknown, MagIdWarn, MagStuck, MagSat, MagUncal, MagCalOtherChip, MagNoStatus,
  I2cStuck, I2cRecovered, KeyStuck, NvsFail, CfgReset, ClockLost, SyncRejected,
  CrashRestored, Brownout, SleepFail, RtcLost, Count };
// Device identities shared by drivers, app and ui (declared here so ui/ never includes drivers/):
enum class ShtChip  : uint8_t { None, Sht4x, Sht3x, Unknown };
enum class MagChip  : uint8_t { None, Qmc5883l, Qmc5883p, Hmc5883l, Unknown };
enum class DrdyKind : uint8_t { None, RisingLevel, FallingPulse };       // QMC5883P has no DRDY pin
struct I2cStats { uint32_t ok, absent, fail, recoveries, recoveryFails; uint8_t consecutiveFail; };
// Same numeric values as esp_reset_reason_t in IDF 5.5 (esp_system.h); hal/hal_power.cpp static_asserts each one (D36).
enum class ResetReason : uint8_t { Unknown = 0, PowerOn = 1, Ext = 2, Sw = 3, Panic = 4, IntWdt = 5, TaskWdt = 6, Wdt = 7,
                                   DeepSleep = 8, Brownout = 9, Sdio = 10, Usb = 11, Jtag = 12, Efuse = 13, PwrGlitch = 14,
                                   CpuLockup = 15 };
// Our own small set; hal maps esp_sleep_get_wakeup_cause() with an explicit switch (anything else -> Other).
enum class WakeCause : uint8_t { None /*not a sleep wake*/, Ext1, Timer, Gpio /*light sleep*/, Other };
struct BatteryState { bool present; uint16_t mv; uint8_t pct; };        // IBattery result; ViewModel.bat
}  // namespace hg
