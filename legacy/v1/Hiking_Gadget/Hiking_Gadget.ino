/*
  Hiking Gadget — ESP32-S3 SuperMini  v1.0
  ─────────────────────────────────────────
  5-screen carousel: Home / Stopwatch / Timer / Compass / Settings

  Required libraries (Arduino IDE → Library Manager):
    Adafruit SSD1306         2.5.x
    Adafruit GFX Library     1.12.x
    U8g2_for_Adafruit_GFX    1.8.x   (font bridge — NOT plain "U8g2")
    Adafruit SHT4x Library   1.0.x
  (QMC5883L is driven by raw I2C in compass_driver.cpp — no extra lib needed.)

  Board: "ESP32S3 Dev Module"  (FQBN esp32:esp32:esp32s3)

  Time sync on boot — run within 3 seconds of power-on:
    python sync_time.py           (auto-detects port)
    python sync_time.py COM3      (Windows)
    python sync_time.py /dev/ttyUSB0  (Linux/Mac)

  Battery: wire voltage divider to GPIO2
    VBAT ──[100k]──┬──[100k]── GND
                   └── GPIO2
  If no battery circuit is wired, battery shows as N/A.
*/

#include "src/config.h"
#include "src/hal/power.h"
#include "src/hal/i2c_buses.h"
#include "src/hal/display.h"
#include "src/hal/buttons.h"
#include "src/hal/battery.h"
#include "src/hal/splash.h"
#include "src/drivers/sht45_driver.h"
#include "src/drivers/compass_driver.h"
#include "src/services/settings_store.h"
#include "src/services/clock.h"
#include "src/services/log.h"
#include "src/services/ui_manager.h"

void setup() {
    Serial.begin(115200);
    // On a cold boot give the USB-CDC host a moment to attach so the banner and
    // I2C scan aren't lost. Skipped on a deep-sleep button wake (must feel instant).
    if (!Power::wokeFromDeepSleep()) {
        uint32_t t0 = millis();
        while (!Serial && (millis() - t0) < 800) { /* wait for host */ }
    }

    Log::begin();             // banner: chip, reset/wake reason, heap

    // Power: cap CPU clock and keep WiFi powered down until explicitly needed
    // (WiFi is the biggest drain on the ~300mAh pack — only on for NTP/portal).
    Power::begin();

    I2CBuses::begin();
    Log::i2cScanAll();        // report what's actually on each bus

    bool haveDisplay = Display::begin();
    if (haveDisplay) LOGI("OLED", "ready at 0x%02X", ADDR_SSD1306);
    else             LOGE("OLED", "not found at 0x%02X - running headless", ADDR_SSD1306);

    // Skip the splash on a button wake from deep sleep — feels instant.
    if (haveDisplay && !Power::wokeFromDeepSleep()) Splash::show(1400);

    Buttons::begin();
    Battery::begin();
    if (Battery::percent() == 255) LOGW("BAT", "no pack detected (ADC floating)");
    else LOGI("BAT", "%.2fV  %u%%", Battery::voltage(), Battery::percent());

    SettingsStore::begin();
    Clock::begin();           // seeds RTC; accepts "T<epoch>\n" on Serial for 3s
    LOGI("CLK", "epoch=%lu synced=%d", (unsigned long)Clock::nowUTC(),
         Clock::isSynced() ? 1 : 0);

    SHT45Driver::begin();
    CompassDriver::begin();

    UIManager::begin();
    LOGI("SYS", "boot complete");
}

void loop() {
    Buttons::poll();
    Battery::tick();
    SHT45Driver::tick();
    CompassDriver::tick();
    UIManager::tick();
    Log::telemetry();         // interval-gated; silent unless USB host attached
}
