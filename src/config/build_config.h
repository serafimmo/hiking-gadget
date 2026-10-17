#pragma once
// Feature flags, bus clocks and the build zone (SPEC Appendix A).
// No #if/#error here: config/ is included by host code, where ESP_ARDUINO_VERSION_MAJOR is undefined (reads as 0).
#include <stddef.h>
#include <stdint.h>
namespace hg {
constexpr uint32_t kSensorI2cHz = 100000, kOledI2cHz = 400000;
constexpr uint16_t kI2cTimeoutMs = 20;
constexpr uint32_t kCpuMhz = 80;
constexpr int16_t  kBuildTzOffsetMin = 180;   constexpr int32_t kBuildToBootLagS = 30;
enum class OledController : uint8_t { Ssd1306, Sh1106 };
constexpr OledController kOledController = OledController::Ssd1306;
constexpr bool kOledFlip = false, kOledFastWake = true;   // kOledFlip: u8g2_SetFlipMode (never the rotated constructor), §6.6
constexpr bool kQmc5883pWriteSignReg = true;    // datasheet setup step (D13)
constexpr bool kDeepSleepKeepRtcPeriph = true;  // D14
constexpr bool kForceLed48Off = true;
constexpr size_t kSerialTxBufBytes = 4096, kLogTxFifoBytes = 1024;   // D31
}  // namespace hg
