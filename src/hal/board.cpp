#include "build_checks.h"
#include "board.h"

#include <esp_idf_version.h>
#include <esp_timer.h>
#include <new>

#include "../app/app_core.h"
#include "../config/board_pins.h"
#include "../config/build_config.h"
#include "hal_ports.h"

// Composition root (SPEC §4.5, §4.6): the hal ports, the AppCore, setup() and loop().
namespace hg {
namespace {
HalMonoClock s_mono;
HalRtcClock s_rtc;
HalWallClock s_wall;
HalKeys s_keys;
HalOled s_oled;
HalKv s_kv;
HalRtcStore s_rtcStore;
HalPower s_power;
HalSerial s_serial;
HalBattery s_battery;
HalDrdy s_drdy;
HalI2c s_bus0(Wire, kPinSensorSda, kPinSensorScl, kSensorI2cHz);
HalI2c s_bus1(Wire1, kPinOledSda, kPinOledScl, kOledI2cHz);
alignas(AppCore) uint8_t s_appMem[sizeof(AppCore)];   // constructed after the display init: no heap
AppCore* s_app = nullptr;
char s_core[32];
}  // namespace

void boardSetup(const char* buildDate, const char* buildTime) {
  // 1. USB CDC console: the TX ring is sized before begin(); never wait for a host
  s_serial.begin();
  // 2. wake info and the key sample (pins released from RTC/hold first)
  uint8_t low = 0;
  releaseKeyPins();
  delayMicroseconds(50);
  for (uint8_t i = 0; i < kKeyCount; ++i) if (digitalRead(kKeyPins[i]) == LOW) low |= (uint8_t)(1u << i);
  s_power.captureWake(low, (uint32_t)esp_timer_get_time());
  // 3. CPU 80 MHz (APB stays 80 MHz: USB and I2C unaffected)
  setCpuFrequencyMhz(kCpuMhz);
  // 4. GPIO48 drives a red LED and a WS2812 DIN on the reference board: off
  if (kForceLed48Off) rgbLedWrite(kPinLed48, 0, 0, 0);
  // 6. I2C buses (bus 0 sensors 100 kHz, bus 1 display 400 kHz), each after a stuck-line check
  s_bus0.begin();
  s_bus1.begin();
  // 7. display: fast init when the panel stayed powered (deep-sleep wake or software reset)
  const ResetReason rr = s_power.wakeInfo().resetReason;
  const bool warm = rr == ResetReason::DeepSleep || rr == ResetReason::Sw;
  if (!s_oled.begin(warm, Brightness::Mid)) {
    if (s_oled.address()) Serial.printf("E OLED E-OLED-IO init failed at 0x%02X on SDA10/SCL11 (running headless)\r\n", s_oled.address());
    else Serial.printf("E OLED E-OLED-NONE no ACK at 0x3C/0x3D on SDA10/SCL11 (running headless)\r\n");
  }
  s_keys.begin();
  s_kv.begin();
  s_power.attach(&s_oled, &s_bus0, &s_bus1, &s_serial);
  const Ports ports{&s_mono, &s_rtc, &s_wall, &s_keys, &s_oled, &s_kv, &s_rtcStore, &s_power, &s_serial,
                    &s_battery, &s_drdy, &s_bus0, &s_bus1};
  s_app = new (s_appMem) AppCore(ports);
  snprintf(s_core, sizeof s_core, "%d.%d.%d / IDF %s", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR,
           ESP_ARDUINO_VERSION_PATCH, esp_get_idf_version());
  // 5, 8-12: settings, clock, sensors, models, boot report, cards, swallow
  s_app->boot(BootInputs{buildDate, buildTime, s_core});
  // 13. loop watchdog (TWDT 5 s)
  enableLoopWDT();
}

void boardLoop() {
  const Directive d = s_app->step();
  switch (d.sleep) {
    case Directive::Sleep::Light:
      s_app->noteLightSleep(s_power.lightSleep(d.lightUs, d.excludeKeys));   // returns on a key or the timer
      break;
    case Directive::Sleep::Deep:
      s_app->onDeepSleepFailed(s_power.deepSleep(d.deep));                   // returns only on failure
      break;
    default:
      if (d.idleMs) delay(d.idleMs);                                         // <= 5 ms; lets idle tasks run
      break;
  }
  if (!s_oled.ok()) s_oled.retryInit();
}

}  // namespace hg
