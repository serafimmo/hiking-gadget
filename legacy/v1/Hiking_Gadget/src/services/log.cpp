#include "log.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdarg.h>
#include <esp_system.h>
#include <esp_sleep.h>
#include "../config.h"
#include "../hal/battery.h"
#include "../drivers/sht45_driver.h"
#include "../drivers/compass_driver.h"
#include "ui_manager.h"
#include "clock.h"

#if LOG_ENABLED

// ── Common line prefix + body ─────────────────────────────────────────────────
static void emit(char lvl, const char* tag, const char* fmt, va_list ap) {
    char buf[180];
    uint32_t ms = millis();
    int n = snprintf(buf, sizeof(buf), "[t+%lu.%03lus][%c][%s] ",
                     (unsigned long)(ms / 1000), (unsigned long)(ms % 1000),
                     lvl, tag);
    if (n < 0) n = 0;
    if (n > (int)sizeof(buf) - 1) n = sizeof(buf) - 1;
    vsnprintf(buf + n, sizeof(buf) - n, fmt, ap);
    Serial.println(buf);
}

void Log::event(const char* tag, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); emit('I', tag, fmt, ap); va_end(ap);
}
void Log::warn(const char* tag, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); emit('W', tag, fmt, ap); va_end(ap);
}
void Log::err(const char* tag, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); emit('E', tag, fmt, ap); va_end(ap);
}

// ── Boot banner ───────────────────────────────────────────────────────────────
static const char* resetReasonStr(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:   return "power-on";
        case ESP_RST_EXT:       return "external";
        case ESP_RST_SW:        return "software";
        case ESP_RST_PANIC:     return "panic";
        case ESP_RST_INT_WDT:   return "int-wdt";
        case ESP_RST_TASK_WDT:  return "task-wdt";
        case ESP_RST_WDT:       return "wdt";
        case ESP_RST_DEEPSLEEP: return "deep-sleep";
        case ESP_RST_BROWNOUT:  return "brownout";
        default:                return "other";
    }
}
static const char* wakeCauseStr(esp_sleep_wakeup_cause_t c) {
    switch (c) {
        case ESP_SLEEP_WAKEUP_EXT1:  return "button (ext1)";
        case ESP_SLEEP_WAKEUP_TIMER: return "timer";
        case ESP_SLEEP_WAKEUP_GPIO:  return "gpio";
        default:                     return "cold boot";
    }
}

void Log::begin() {
    Serial.println();
    Serial.println(F("======================================================"));
    Serial.println(F("  HIKE GADGET  ESP32-S3   diagnostic log"));
    Serial.printf ("  chip: %s rev%d  cores:%d  cpu:%dMHz\n",
                   ESP.getChipModel(), ESP.getChipRevision(),
                   ESP.getChipCores(), ESP.getCpuFreqMHz());
    Serial.printf ("  reset: %s   wake: %s\n",
                   resetReasonStr(esp_reset_reason()),
                   wakeCauseStr(esp_sleep_get_wakeup_cause()));
    Serial.printf ("  flash: %u KB   free heap: %u B\n",
                   (unsigned)(ESP.getFlashChipSize() / 1024),
                   (unsigned)ESP.getFreeHeap());
    Serial.println(F("======================================================"));
}

// ── I2C bus scan ────────────────────────────────────────────────────────────
static const char* knownDevice(uint8_t addr) {
    switch (addr) {
        case ADDR_SSD1306:  return "SSD1306 OLED";
        case ADDR_QMC5883L: return "QMC5883L compass";
        case 0x44:          return "SHT45 temp/humidity";
        default:            return nullptr;
    }
}

static bool present(TwoWire& bus, uint8_t addr) {
    bus.beginTransmission(addr);
    return bus.endTransmission() == 0;
}

static void scanBus(TwoWire& bus, const char* name) {
    Serial.printf("[I2C] scanning %s:\n", name);
    uint8_t count = 0;
    for (uint8_t a = 1; a < 127; a++) {
        if (present(bus, a)) {
            const char* k = knownDevice(a);
            Serial.printf("       0x%02X  %s\n", a, k ? k : "(unknown)");
            count++;
        }
    }
    if (count == 0) Serial.println(F("       (no devices found)"));
}

void Log::i2cScanAll() {
    scanBus(Wire,  "Wire  (sensors SCL=7/SDA=8)");
    scanBus(Wire1, "Wire1 (display SCL=11/SDA=10)");

    // Explicit present/MISSING summary for the three expected devices.
    Serial.printf("[I2C] OLED 0x%02X:%s  SHT45 0x44:%s  compass 0x%02X:%s\n",
        ADDR_SSD1306,  present(Wire1, ADDR_SSD1306)  ? "OK" : "MISSING",
                       present(Wire,  0x44)          ? "OK" : "MISSING",
        ADDR_QMC5883L, present(Wire,  ADDR_QMC5883L) ? "OK" : "MISSING");
}

// ── Periodic telemetry ────────────────────────────────────────────────────────
static float dewPoint(float t, float rh) {
    if (rh < 1.0f) rh = 1.0f;
    const float a = 17.62f, b = 243.12f;
    float g = logf(rh / 100.0f) + (a * t) / (b + t);
    return (b * g) / (a - g);
}

static const char* screenName(ScreenID s) {
    switch (s) {
        case ScreenID::HOME:      return "HOME";
        case ScreenID::STOPWATCH: return "STOPWATCH";
        case ScreenID::TIMER:     return "TIMER";
        case ScreenID::COMPASS:   return "COMPASS";
        case ScreenID::SETTINGS:  return "SETTINGS";
        default:                  return "?";
    }
}

void Log::telemetry() {
    static uint32_t last = 0;
    if (!Serial) return;                 // USB-CDC host not attached → stay silent
    uint32_t now = millis();
    if (last != 0 && (now - last) < LOG_TELEMETRY_MS) return;
    last = now;

    SensorData  sd = SHT45Driver::getData();
    CompassData cd = CompassDriver::getData();

    char tbuf[40];
    if (sd.valid)
        snprintf(tbuf, sizeof(tbuf), "t=%.1fC rh=%.0f%% dp=%.1fC",
                 sd.temperature, sd.humidity,
                 dewPoint(sd.temperature, sd.humidity));
    else
        snprintf(tbuf, sizeof(tbuf), "t=--- rh=--- dp=---");

    char cbuf[60];
    if (cd.valid)
        snprintf(cbuf, sizeof(cbuf), "hdg=%03.0f raw=%.1f xyz=%d,%d,%d",
                 cd.angle, cd.target, cd.rawX, cd.rawY, cd.rawZ);
    else
        snprintf(cbuf, sizeof(cbuf), "hdg=--- (no signal)");

    char bbuf[24];
    if (Battery::percent() == 255)
        snprintf(bbuf, sizeof(bbuf), "bat=n/a");
    else
        snprintf(bbuf, sizeof(bbuf), "bat=%.2fV %u%%",
                 Battery::voltage(), Battery::percent());

    Serial.printf("[t+%lu.%03lus][TLM] %s | %s | %s | scr=%s sync=%d heap=%u\n",
                  (unsigned long)(now / 1000), (unsigned long)(now % 1000),
                  tbuf, cbuf, bbuf,
                  screenName(UIManager::currentScreen()),
                  Clock::isSynced() ? 1 : 0,
                  (unsigned)ESP.getFreeHeap());
}

#else  // LOG_ENABLED == 0 — empty stubs so callers still link
void Log::begin() {}
void Log::i2cScanAll() {}
void Log::event(const char*, const char*, ...) {}
void Log::warn (const char*, const char*, ...) {}
void Log::err  (const char*, const char*, ...) {}
void Log::telemetry() {}
#endif
