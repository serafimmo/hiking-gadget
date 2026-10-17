#pragma once
// Pins, I2C addresses and the key map (SPEC Appendix A). Constants only, no code.
#include <stdint.h>
namespace hg {
constexpr uint8_t kPinSensorSda = 8, kPinSensorScl = 7;        // Wire  (SHT + magnetometer)
constexpr uint8_t kPinOledSda   = 10, kPinOledScl  = 11;       // Wire1 (SSD1306)
constexpr uint8_t kPinMagDrdy   = 12;                          // diagnostics only
constexpr uint8_t kKeyPins[4]   = { 4 /*K1 ^*/, 5 /*K2 v*/, 6 /*K3 #*/, 1 /*K4 * */ };   // [HW?] verify with Diagnostics > Buttons
// ext1 deep-sleep wake set, derived from kKeyPins so that editing the key map can never desync it (0x72 by default)
constexpr uint64_t kWakeMask    = (1ULL << kKeyPins[0]) | (1ULL << kKeyPins[1]) | (1ULL << kKeyPins[2]) | (1ULL << kKeyPins[3]);
constexpr int8_t  kPinBatteryAdc = -1;  constexpr float kBatteryDividerRatio = 2.0f;   // GPIO2 suggested
constexpr uint8_t kPinLed48 = 48;
constexpr uint8_t kShtAddrs[2] = {0x44, 0x45};
constexpr uint8_t kMagAddrQmcL = 0x0D, kMagAddrQmcP = 0x2C, kMagAddrHmc = 0x1E;
constexpr uint8_t kOledAddrs[2] = {0x3C, 0x3D};
static_assert(kKeyPins[0] <= 21 && kKeyPins[1] <= 21 && kKeyPins[2] <= 21 && kKeyPins[3] <= 21,
              "key pins must be RTC GPIOs (0..21 on the S3): ext1 deep-sleep wake works only on those");
static_assert(__builtin_popcountll(kWakeMask) == 4, "kKeyPins must name four different pins");
}  // namespace hg
