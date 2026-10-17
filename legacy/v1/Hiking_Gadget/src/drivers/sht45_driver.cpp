#include "sht45_driver.h"
#include <Adafruit_SHT4x.h>
#include "../config.h"
#include "../services/settings_store.h"
#include "../services/log.h"

static Adafruit_SHT4x sht4x;
static SensorData lastReading   = {0.0f, 0.0f, false};
static uint32_t   lastPollMs    = 0;
static uint32_t   lastHeaterMs  = 0;
static bool       initialized   = false;

void SHT45Driver::begin() {
    initialized = sht4x.begin(&Wire);
    if (initialized) {
        sht4x.setPrecision(SHT4X_HIGH_PRECISION);
        sht4x.setHeater(SHT4X_NO_HEATER);
        LOGI("SHT45", "ready at 0x44");
    } else {
        LOGE("SHT45", "not found at 0x44");
    }
}

void SHT45Driver::tick() {
    if (!initialized) return;
    uint32_t now = millis();
    if (lastHeaterMs == 0) lastHeaterMs = now;

    // Periodic anti-condensation heater pulse (if enabled in settings).
    // The SHT4x heats only during a measurement command, so we trigger one
    // heated read, discard it (the reading is hot), and force a fresh normal
    // poll next. This blocks ~1.1s but only fires every N minutes and is OFF
    // by default. K3/K4 nav still works since events are queued, not lost.
    uint32_t heaterMs = (uint32_t)gSettings.sht45HeaterIntervalMin * 60000UL;
    if (heaterMs > 0 && (now - lastHeaterMs) >= heaterMs) {
        LOGI("SHT45", "heater pulse (anti-condensation)");
        sht4x.setHeater(SHT4X_HIGH_HEATER_1S);
        sensors_event_t h, t;
        sht4x.getEvent(&h, &t);          // heated measurement (discarded)
        sht4x.setHeater(SHT4X_NO_HEATER);
        lastHeaterMs = now;
        lastPollMs   = 0;                // force a fresh normal read below
    }

    // Regular poll every SENSOR_POLL_MS
    if (lastPollMs == 0 || (now - lastPollMs) >= SENSOR_POLL_MS) {
        sensors_event_t h, t;
        if (sht4x.getEvent(&h, &t)) {
            bool wasValid = lastReading.valid;
            lastReading = {t.temperature, h.relative_humidity, true};
            if (!wasValid)
                LOGI("SHT45", "first reading %.1fC %.0f%%",
                     t.temperature, h.relative_humidity);
        }
        lastPollMs = millis();
    }
}

SensorData SHT45Driver::getData() { return lastReading; }
