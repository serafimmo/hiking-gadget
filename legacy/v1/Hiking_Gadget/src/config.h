#pragma once

// ── Sensor I2C Bus (Wire) ─────────────────────────────────────────────────────
#define PIN_SCL_SENSOR   7
#define PIN_SDA_SENSOR   8

// ── Display I2C Bus (Wire1) ───────────────────────────────────────────────────
#define PIN_SCL_DISPLAY  11
#define PIN_SDA_DISPLAY  10

// ── Buttons (active LOW, INPUT_PULLUP) ────────────────────────────────────────
// WARNING: GPIO1 on ESP32-S3 = USB D−. If USB-Serial is active, move K4 to GPIO3.
// (GPIO2 is taken by the battery ADC divider, so use GPIO3 as the K4 fallback.)
#define PIN_K1  4
#define PIN_K2  5
#define PIN_K3  6
#define PIN_K4  1

// ── Battery sense (voltage divider 100k/100k → half of VBAT) ──────────────────
#define PIN_BATTERY_ADC  2      // GPIO2 = ADC1_CH1 (works while WiFi is on)

// ── Compass DRDY ─────────────────────────────────────────────────────────────
#define PIN_COMPASS_DRDY  12

// ── I2C Addresses ────────────────────────────────────────────────────────────
#define ADDR_SSD1306   0x3C
#define ADDR_QMC5883L  0x0D

// ── Display ──────────────────────────────────────────────────────────────────
#define OLED_WIDTH   128
#define OLED_HEIGHT  64

// ── Button timing (ms) ───────────────────────────────────────────────────────
#define DEBOUNCE_MS      30
#define LONG_PRESS_MS   700

// ── Sensor polling interval ───────────────────────────────────────────────────
#define SENSOR_POLL_MS  5000UL

// ── Clock sync window on boot ────────────────────────────────────────────────
#define CLOCK_SYNC_TIMEOUT_MS  3000UL

// ── Settings defaults ─────────────────────────────────────────────────────────
#define DEFAULT_TZ_INDEX              1    // MSK+3
#define DEFAULT_DISPLAY_TIMEOUT_SEC  20
#define DEFAULT_HEATER_INTERVAL_MIN   0    // off
#define DEFAULT_LOW_POWER             1    // light-sleep when idle

// ── Power management ──────────────────────────────────────────────────────────
#define CPU_FREQ_MHZ          80     // 80MHz is plenty for this UI, saves ~30%
#define IDLE_SLEEP_MS        400     // light-sleep slice when screen is off
#define BATTERY_POLL_MS    30000UL   // ADC read cadence
#define AUTO_DEEP_SLEEP_SEC  600     // idle this long (and not busy) → deep sleep

// ── WiFi setup portal ─────────────────────────────────────────────────────────
#define WIFI_AP_SSID      "HikeGadget-Setup"
#define WIFI_AP_PASS      "hike1234"        // >= 8 chars
#define WIFI_CONNECT_TIMEOUT_MS  12000UL    // give up joining home WiFi after this
#define NTP_SERVER_1      "pool.ntp.org"
#define NTP_SERVER_2      "time.google.com"

// ── EEPROM ───────────────────────────────────────────────────────────────────
#define SETTINGS_EEPROM_VERSION  0x05
#define EEPROM_SIZE  64

// ── Serial diagnostics / logging ──────────────────────────────────────────────
#define LOG_ENABLED        1         // set 0 to compile out all logging
#define LOG_TELEMETRY_MS   2000UL    // sensor snapshot cadence (when USB attached)
