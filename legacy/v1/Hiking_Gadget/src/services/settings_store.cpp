#include "settings_store.h"
#include <EEPROM.h>
#include <string.h>
#include "../config.h"

const TZEntry TZ_LIST[] = {
    {"UTC",    0},
    {"MSK",  180},
    {"CET",   60},
    {"CEST", 120},
    {"EET",  120},
    {"EST",  -300},
    {"EDT",  -240},
    {"CST",  -360},
    {"CDT",  -300},
    {"PST",  -480},
    {"PDT",  -420},
    {"JST",   540},
    {"IST",   330},
    {"CST8",  480},
};
const uint8_t TZ_COUNT = sizeof(TZ_LIST) / sizeof(TZ_LIST[0]);

AppSettings gSettings;

static uint16_t crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 1) ? ((crc >> 1) ^ 0xA001) : (crc >> 1);
    }
    return crc;
}

AppSettings SettingsStore::defaults() {
    AppSettings s = {};
    s.version                = SETTINGS_EEPROM_VERSION;
    s.tzIndex                = DEFAULT_TZ_INDEX;
    s.displayTimeoutSec      = DEFAULT_DISPLAY_TIMEOUT_SEC;
    s.sht45HeaterIntervalMin = DEFAULT_HEATER_INTERVAL_MIN;
    s.compassDeclinationDeg  = 0.0f;
    s.lastEpoch              = 0;
    s.timerPresetIndex       = 0;
    s.compassOffX            = 0;
    s.compassOffY            = 0;
    s.lowPower               = DEFAULT_LOW_POWER;
    return s;
}

void SettingsStore::begin() {
    EEPROM.begin(EEPROM_SIZE);
    load();
}

void SettingsStore::load() {
    AppSettings s;
    EEPROM.get(0, s);
    size_t dataLen = sizeof(AppSettings) - sizeof(uint16_t);
    uint16_t computed = crc16((const uint8_t*)&s, dataLen);
    if (s.version != SETTINGS_EEPROM_VERSION || s.crc != computed) {
        gSettings = defaults();
        save();
    } else {
        gSettings = s;
    }
}

void SettingsStore::save() {
    AppSettings tmp = gSettings;
    size_t dataLen  = sizeof(AppSettings) - sizeof(uint16_t);
    tmp.crc = crc16((const uint8_t*)&tmp, dataLen);

    // Skip the flash write if nothing actually changed — protects flash from
    // wear when screens call save() on every onExit().
    AppSettings cur;
    EEPROM.get(0, cur);
    if (memcmp(&cur, &tmp, sizeof(AppSettings)) == 0) return;

    EEPROM.put(0, tmp);
    EEPROM.commit();
}

const char* SettingsStore::tzName(uint8_t i) {
    return (i < TZ_COUNT) ? TZ_LIST[i].name : "???";
}

int16_t SettingsStore::tzOffsetMin(uint8_t i) {
    return (i < TZ_COUNT) ? TZ_LIST[i].offsetMin : 0;
}
