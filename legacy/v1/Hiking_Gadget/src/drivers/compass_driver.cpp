#include "compass_driver.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "../config.h"
#include "../services/settings_store.h"
#include "../services/clock.h"
#include "../services/log.h"

// QMC5883L register map
#define QMC_REG_DATA    0x00
#define QMC_REG_STATUS  0x06
#define QMC_REG_CTRL1   0x09
#define QMC_REG_CTRL2   0x0A
#define QMC_REG_PERIOD  0x0B
#define QMC_ADDR        ADDR_QMC5883L

static volatile bool dataReady = false;
static CompassData   cdata     = {0.0f, 0.0f, false, 0, 0, 0};
static float         prevAngle = -999.0f;

static void IRAM_ATTR drdyISR() { dataReady = true; }

static void writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(QMC_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}

static bool readXYZ(int16_t &x, int16_t &y, int16_t &z) {
    Wire.beginTransmission(QMC_ADDR);
    Wire.write(QMC_REG_DATA);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)QMC_ADDR, (uint8_t)6) != 6) return false;
    // QMC5883L: LSB first, MSB second — must read in defined order
    uint8_t xl = Wire.read(), xh = Wire.read();
    uint8_t yl = Wire.read(), yh = Wire.read();
    uint8_t zl = Wire.read(), zh = Wire.read();
    x = (int16_t)(xl | ((uint16_t)xh << 8));
    y = (int16_t)(yl | ((uint16_t)yh << 8));
    z = (int16_t)(zl | ((uint16_t)zh << 8));
    return true;
}

void CompassDriver::begin() {
    Wire.beginTransmission(QMC_ADDR);
    bool found = (Wire.endTransmission() == 0);
    if (found) LOGI("COMPASS", "QMC5883L ready at 0x%02X", QMC_ADDR);
    else       LOGE("COMPASS", "QMC5883L not found at 0x%02X", QMC_ADDR);

    // Soft reset, then configure
    writeReg(QMC_REG_CTRL2, 0x80); // SOFT_RST
    delay(10);
    writeReg(QMC_REG_PERIOD, 0x01);
    // OSR=512, RNG=2G, ODR=200Hz, Mode=Continuous
    writeReg(QMC_REG_CTRL1, 0b00001101);
    // Enable DRDY interrupt
    writeReg(QMC_REG_CTRL2, 0x01);

    pinMode(PIN_COMPASS_DRDY, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_COMPASS_DRDY), drdyISR, RISING);
}

void CompassDriver::tick() {
    // Read on DRDY interrupt, OR poll at ~10Hz as a fallback when the DRDY
    // line is dead (common on cheap GY-273 clones).
    static uint32_t lastReadMs = 0;
    uint32_t now = Clock::uptimeMs();
    if (!dataReady && (now - lastReadMs) < 100) return;
    dataReady  = false;
    lastReadMs = now;

    int16_t x, y, z;
    if (!readXYZ(x, y, z)) return;

    cdata.rawX = x;
    cdata.rawY = y;
    cdata.rawZ = z;

    // Apply hard-iron calibration offsets, then compute heading.
    // atan2(Y, X): X points North, Y points East → 0=N, 90=E, clockwise.
    float fx = (float)(x - gSettings.compassOffX);
    float fy = (float)(y - gSettings.compassOffY);
    float raw = atan2f(fy, fx) * 180.0f / M_PI;
    if (raw < 0)     raw += 360.0f;
    if (raw >= 360)  raw -= 360.0f;

    // Apply magnetic declination
    raw += gSettings.compassDeclinationDeg;
    if (raw < 0)     raw += 360.0f;
    if (raw >= 360)  raw -= 360.0f;

    cdata.target = raw;

    // Exponential smoothing with shortest-path angle interpolation
    if (!cdata.valid) {
        cdata.angle = raw;
    } else {
        float diff = cdata.target - cdata.angle;
        if (diff >  180.0f) diff -= 360.0f;
        if (diff < -180.0f) diff += 360.0f;
        cdata.angle += diff * 0.10f;
        if (cdata.angle < 0)     cdata.angle += 360.0f;
        if (cdata.angle >= 360)  cdata.angle -= 360.0f;
    }
    cdata.valid = true;
}

bool CompassDriver::angleChanged() {
    if (!cdata.valid) return false;
    float diff = cdata.angle - prevAngle;
    if (diff < 0) diff = -diff;
    if (diff > 180) diff = 360 - diff;
    if (diff >= 0.5f) {
        prevAngle = cdata.angle;
        return true;
    }
    return false;
}

CompassData CompassDriver::getData() { return cdata; }

// ── Hard-iron calibration ─────────────────────────────────────────────────────
static int16_t cMinX, cMaxX, cMinY, cMaxY;

void CompassDriver::calibStart() {
    cMinX = cMinY = 32767;
    cMaxX = cMaxY = -32768;
}

void CompassDriver::calibSample() {
    int16_t x, y, z;
    if (!readXYZ(x, y, z)) return;
    if (x < cMinX) cMinX = x;  if (x > cMaxX) cMaxX = x;
    if (y < cMinY) cMinY = y;  if (y > cMaxY) cMaxY = y;
}

int16_t CompassDriver::calibSpan() {
    int32_t spanX = (int32_t)cMaxX - cMinX;
    int32_t spanY = (int32_t)cMaxY - cMinY;
    int32_t s = (spanX < spanY) ? spanX : spanY;
    if (s < 0) s = 0;
    return (int16_t)s;
}

void CompassDriver::calibApply() {
    if (cMaxX <= cMinX || cMaxY <= cMinY) {
        LOGW("COMPASS", "calibration skipped (no data)");
        return;   // no data, keep old offsets
    }
    gSettings.compassOffX = (int16_t)(((int32_t)cMinX + cMaxX) / 2);
    gSettings.compassOffY = (int16_t)(((int32_t)cMinY + cMaxY) / 2);
    SettingsStore::save();
    LOGI("COMPASS", "calibrated offX=%d offY=%d span=%d",
         gSettings.compassOffX, gSettings.compassOffY, calibSpan());
}
