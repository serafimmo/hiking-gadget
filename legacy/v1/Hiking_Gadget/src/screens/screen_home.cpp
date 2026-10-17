#include "screen_home.h"
#include "../hal/display.h"
#include "../hal/ui.h"
#include "../hal/battery.h"
#include "../services/clock.h"
#include "../services/settings_store.h"
#include "../drivers/sht45_driver.h"
#include <math.h>
#include <stdio.h>

// Steadman simplified heat index — "feels like" (meaningful above 26C / 40% RH)
static float heatIndex(float t, float rh) {
    if (t < 26.0f || rh < 40.0f) return t;
    return -8.784695f + 1.61139411f * t + 2.338549f * rh
           - 0.14611605f * t * rh - 0.01230809f * t * t
           - 0.01642482f * rh * rh + 0.00221173f * t * t * rh
           + 0.00072546f * t * rh * rh - 0.00000358f * t * t * rh * rh;
}

// Magnus dew point — the temperature at which dew/condensation forms.
// Practical on a trail: tent/gear condensation, fog risk, sleeping comfort.
static float dewPoint(float t, float rh) {
    if (rh < 1.0f) rh = 1.0f;
    const float a = 17.62f, b = 243.12f;
    float gamma = logf(rh / 100.0f) + (a * t) / (b + t);
    return (b * gamma) / (a - gamma);
}

// Draw "<value>°C" with a real round degree glyph, return total pixel width.
static void drawTempC(int16_t x, int16_t baseline, float t) {
    char num[8];
    snprintf(num, sizeof(num), "%.1f", t);
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.setCursor(x, baseline);
    u8g2.print(num);
    int16_t nx = x + u8g2.getUTF8Width(num);
    // small degree ring just above mid-line
    oled.drawCircle(nx + 3, baseline - 9, 2, WHITE);
    u8g2.setCursor(nx + 7, baseline);
    u8g2.print("C");
}

void ScreenHome::onEnter() {
    lastSec = 255;
    _dirty  = true;
}

void ScreenHome::tick() {
    DateTime dt = Clock::toDateTime(Clock::nowLocal());
    if (dt.second != lastSec) { lastSec = dt.second; _dirty = true; }
    uint8_t bat = Battery::percent();
    if (bat != lastBat) { lastBat = bat; _dirty = true; }
}

void ScreenHome::render() {
    DateTime   dt = Clock::toDateTime(Clock::nowLocal());
    SensorData sd = SHT45Driver::getData();

    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);

    // ── Top-left: timezone / sync state (top bar reserves centre+right) ───
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(0, UI::TOP_BAR_Y);
    if (!Clock::isSynced()) {
        u8g2.print("SYNC?");
    } else {
        u8g2.print(SettingsStore::tzName(gSettings.tzIndex));
        // Compact climate hint: heat-index "feels like" when it's hot,
        // otherwise dew point (condensation/fog cue). Fits left of the dots.
        if (sd.valid) {
            char fb[12];
            float hi = heatIndex(sd.temperature, sd.humidity);
            if (hi - sd.temperature >= 2.0f) {
                snprintf(fb, sizeof(fb), " HI%.0f", hi);          // feels-like
            } else {
                snprintf(fb, sizeof(fb), " DP%.0f",
                         dewPoint(sd.temperature, sd.humidity));  // dew point
            }
            u8g2.print(fb);
        }
    }
    oled.drawFastHLine(0, UI::TOP_SEP_Y, OLED_WIDTH, WHITE);

    // ── Time (hero, large, centred) ───────────────────────────────────────
    char timeBuf[9];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
             dt.hour, dt.minute, dt.second);
    u8g2.setFont(u8g2_font_logisoso16_tn);
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(timeBuf)), 30);
    u8g2.print(timeBuf);

    // ── Date ──────────────────────────────────────────────────────────────
    char dateBuf[20];
    snprintf(dateBuf, sizeof(dateBuf), "%s  %02d.%02d.%04d",
             Clock::weekdayShort(dt.weekday), dt.day, dt.month, dt.year);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(dateBuf)), 45);
    u8g2.print(dateBuf);

    // ── Temperature (left) & Humidity (right), bottom row, with icons ─────
    if (sd.valid) {
        UI::iconThermo(1, 52);
        drawTempC(8, 63, sd.temperature);
        char humBuf[8];
        snprintf(humBuf, sizeof(humBuf), "%.0f%%", sd.humidity);
        u8g2.setFont(u8g2_font_7x14B_tf);
        int16_t hw = u8g2.getUTF8Width(humBuf);
        u8g2.setCursor(OLED_WIDTH - hw - 2, 63);
        u8g2.print(humBuf);
        UI::iconDrop(OLED_WIDTH - hw - 9, 54);
    } else {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setCursor(2, 62);
        u8g2.print("sensor: --.-C  --%");
    }
}
