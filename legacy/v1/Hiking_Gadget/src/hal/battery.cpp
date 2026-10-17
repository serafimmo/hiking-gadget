#include "battery.h"
#include "../config.h"
#include <Arduino.h>

#define BAT_SAMPLES   8           // average to reduce ADC noise

// LiPo discharge curve endpoints
#define BAT_V_MIN  3.30f          // ~empty (cut-off margin for a 300mAh cell)
#define BAT_V_MAX  4.20f          // full
#define DIVIDER    0.5f           // 100k/100k → pin sees half of VBAT

static uint8_t  _percent  = 255;  // 255 = no battery wired / not measured
static float    _voltage  = 0.0f;
static uint32_t _lastMs   = 0;
static bool     _available = false;

// LiPo voltage→% is non-linear; this piecewise table tracks a typical cell far
// better than a straight line (avoids the "stuck at 100% then plummets" effect).
static uint8_t pctFromVoltage(float v) {
    static const float VT[] = {3.30,3.50,3.60,3.70,3.75,3.80,3.85,3.90,3.95,4.00,4.10,4.20};
    static const float PT[] = {0,   5,   10,  20,  30,  45,  55,  65,  75,  85,  95,  100};
    const uint8_t N = sizeof(VT) / sizeof(VT[0]);
    if (v <= VT[0])   return 0;
    if (v >= VT[N-1]) return 100;
    for (uint8_t i = 1; i < N; i++) {
        if (v < VT[i]) {
            float f = (v - VT[i-1]) / (VT[i] - VT[i-1]);
            return (uint8_t)(PT[i-1] + f * (PT[i] - PT[i-1]));
        }
    }
    return 100;
}

static float readVoltage() {
    uint32_t mv = 0;
    for (uint8_t i = 0; i < BAT_SAMPLES; i++) {
        mv += analogReadMilliVolts(PIN_BATTERY_ADC);  // factory-calibrated mV
        delayMicroseconds(50);
    }
    float pinV = (float)(mv / BAT_SAMPLES) / 1000.0f;
    return pinV / DIVIDER;
}

void Battery::begin() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // full-scale ~3.1V on pin → up to ~6.2V VBAT

    // Detect whether a divider/battery is actually present: a floating ADC pin
    // reads near 0. With the 100k/100k divider a LiPo always reads > ~1.4V.
    float v = readVoltage();
    _available = (v > 2.5f && v < 5.5f);
    if (_available) { _voltage = v; _percent = pctFromVoltage(v); }
}

void Battery::tick() {
    if (!_available) return;
    uint32_t now = millis();
    if (_lastMs != 0 && (now - _lastMs) < BATTERY_POLL_MS) return;
    _lastMs  = now;
    _voltage = readVoltage();
    _percent = pctFromVoltage(_voltage);
}

uint8_t Battery::percent() { return _percent; }
float   Battery::voltage() { return _voltage; }
