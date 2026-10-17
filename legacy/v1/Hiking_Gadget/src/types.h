#pragma once
#include <stdint.h>

enum class ScreenID : uint8_t {
    HOME      = 0,
    STOPWATCH = 1,
    TIMER     = 2,
    COMPASS   = 3,
    SETTINGS  = 4,
    COUNT     = 5
};

enum class ButtonEvent : uint8_t {
    NONE     = 0,
    K1_SHORT = 1,
    K2_SHORT = 2,
    K3_SHORT = 3,
    K4_SHORT = 4,
    K1_LONG  = 5,
    K2_LONG  = 6,
    K3_LONG  = 7
};

struct AppSettings {
    uint8_t  version;
    uint8_t  tzIndex;
    uint16_t displayTimeoutSec;
    uint8_t  sht45HeaterIntervalMin;
    float    compassDeclinationDeg;
    uint32_t lastEpoch;
    uint8_t  timerPresetIndex;
    int16_t  compassOffX;          // hard-iron calibration offset (raw units)
    int16_t  compassOffY;
    uint8_t  lowPower;             // 1 = light-sleep when idle, 0 = always-on
    uint8_t  _pad[3];
    uint16_t crc;
};

struct SensorData {
    float temperature;
    float humidity;
    bool  valid;
};

struct CompassData {
    float   angle;    // smoothed display angle [0..360)
    float   target;   // raw computed heading
    bool    valid;
    int16_t rawX, rawY, rawZ;
};

struct DateTime {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  weekday;  // 0=Sun
};
