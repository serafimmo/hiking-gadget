#include "screen_settings.h"
#include "../hal/display.h"
#include "../hal/ui.h"
#include "../services/settings_store.h"
#include "../services/clock.h"
#include "../services/net.h"
#include "../hal/power.h"
#include "../drivers/compass_driver.h"
#include <stdio.h>

const uint16_t ScreenSettings::TIMEOUT_OPTS[]  = {0, 15, 30, 60, 120, 300};
const uint8_t  ScreenSettings::TIMEOUT_COUNT   = 6;
const uint8_t  ScreenSettings::HEATER_OPTS[]   = {0, 5, 10, 30, 60};
const uint8_t  ScreenSettings::HEATER_COUNT    = 5;

#define CALIB_DURATION_MS  25000UL

void ScreenSettings::onEnter() {
    mode         = Mode::LIST;
    selectedItem = 0;
    scrollTop    = 0;
    _dirty       = true;
}

void ScreenSettings::onExit() {
    SettingsStore::save();
}

// ── Compose label + value text for a menu item ────────────────────────────────
void ScreenSettings::itemText(uint8_t item, const char*& label, char* val, uint8_t vlen) {
    switch (item) {
        case IT_TZ:
            label = "Timezone";
            snprintf(val, vlen, "%s", SettingsStore::tzName(gSettings.tzIndex));
            break;
        case IT_TIMEOUT:
            label = "Timeout";
            if (gSettings.displayTimeoutSec == 0) snprintf(val, vlen, "Never");
            else snprintf(val, vlen, "%ds", gSettings.displayTimeoutSec);
            break;
        case IT_HEATER:
            label = "Heater";
            if (gSettings.sht45HeaterIntervalMin == 0) snprintf(val, vlen, "OFF");
            else snprintf(val, vlen, "%dm", gSettings.sht45HeaterIntervalMin);
            break;
        case IT_LOWPOWER:
            label = "Low power";
            snprintf(val, vlen, "%s", gSettings.lowPower ? "ON" : "OFF");
            break;
        case IT_DECL:
            label = "Declination";
            snprintf(val, vlen, "%+.0f", gSettings.compassDeclinationDeg);
            break;
        case IT_CALIB:    label = "Calibrate compass"; snprintf(val, vlen, ">"); break;
        case IT_SYNC:     label = "Sync time (WiFi)";  snprintf(val, vlen, ">"); break;
        case IT_WIFI:     label = "WiFi setup";        snprintf(val, vlen, ">"); break;
        case IT_POWEROFF: label = "Power off";         snprintf(val, vlen, ">"); break;
        default:       label = "?";                 val[0] = 0;               break;
    }
}

// ── Activate (K2) a menu item ─────────────────────────────────────────────────
void ScreenSettings::activate(uint8_t item) {
    switch (item) {
        case IT_TZ:
            gSettings.tzIndex = (gSettings.tzIndex + 1) % TZ_COUNT;
            break;
        case IT_TIMEOUT: {
            uint8_t i = findIdx(TIMEOUT_OPTS, TIMEOUT_COUNT, gSettings.displayTimeoutSec);
            gSettings.displayTimeoutSec = TIMEOUT_OPTS[(i + 1) % TIMEOUT_COUNT];
            break;
        }
        case IT_HEATER: {
            uint8_t i = findIdx(HEATER_OPTS, HEATER_COUNT, gSettings.sht45HeaterIntervalMin);
            gSettings.sht45HeaterIntervalMin = HEATER_OPTS[(i + 1) % HEATER_COUNT];
            break;
        }
        case IT_LOWPOWER:
            gSettings.lowPower = !gSettings.lowPower;
            break;
        case IT_DECL:
            gSettings.compassDeclinationDeg += 1.0f;
            if (gSettings.compassDeclinationDeg > 180.0f)
                gSettings.compassDeclinationDeg = -180.0f;
            break;
        case IT_CALIB:
            CompassDriver::calibStart();
            calibStartMs = Clock::uptimeMs();
            lastSampleMs = 0;
            mode = Mode::CALIBRATING;
            break;
        case IT_SYNC:
            Net::syncTimeNow();     // blocking, draws its own status on OLED
            break;
        case IT_WIFI:
            Net::startPortal();     // blocking config portal
            break;
        case IT_POWEROFF:
            SettingsStore::save();  // persist before powering down
            Power::deepSleep();     // does not return
            break;
    }
    _dirty = true;
}

void ScreenSettings::onButton(ButtonEvent ev) {
    if (mode == Mode::CALIBRATING) {
        if (ev == ButtonEvent::K4_SHORT) { CompassDriver::calibApply(); mode = Mode::LIST; _dirty = true; }
        else if (ev == ButtonEvent::K3_SHORT) { mode = Mode::LIST; _dirty = true; }
        return;
    }

    // LIST mode
    if (ev == ButtonEvent::K3_SHORT) {
        selectedItem = (selectedItem + 1) % ITEM_COUNT;
        if (selectedItem < scrollTop)            scrollTop = selectedItem;
        if (selectedItem >= scrollTop + VISIBLE) scrollTop = selectedItem - (VISIBLE - 1);
        if (selectedItem == 0)                   scrollTop = 0;   // wrapped to top
        _dirty = true;
    } else if (ev == ButtonEvent::K4_SHORT) {
        activate(selectedItem);
    } else if (ev == ButtonEvent::K3_LONG) {
        if (selectedItem == IT_DECL) {           // long-press: decrement declination
            gSettings.compassDeclinationDeg -= 1.0f;
            if (gSettings.compassDeclinationDeg < -180.0f)
                gSettings.compassDeclinationDeg = 180.0f;
            _dirty = true;
        }
    }
}

void ScreenSettings::tick() {
    if (mode != Mode::CALIBRATING) return;
    uint32_t now = Clock::uptimeMs();
    CompassDriver::calibSample();
    if (now - lastSampleMs >= 150) { lastSampleMs = now; _dirty = true; }
    if (now - calibStartMs >= CALIB_DURATION_MS) {
        CompassDriver::calibApply();
        mode   = Mode::LIST;
        _dirty = true;
    }
}

// ── Rendering ─────────────────────────────────────────────────────────────────
void ScreenSettings::renderList() {
    UI::header("SETTINGS");

    const uint8_t Y0 = 20, STEP = 10;
    for (uint8_t row = 0; row < VISIBLE; row++) {
        uint8_t item = scrollTop + row;
        if (item >= ITEM_COUNT) break;
        uint8_t y = Y0 + row * STEP;
        bool sel  = (item == selectedItem);

        const char* label; char val[14];
        itemText(item, label, val, sizeof(val));

        // Highlight stops short of the right edge, leaving a scrollbar lane.
        if (sel) {
            oled.fillRect(0, y - 8, OLED_WIDTH - 4, STEP, WHITE);
            u8g2.setForegroundColor(BLACK);
        } else {
            u8g2.setForegroundColor(WHITE);
        }
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setFontMode(1);
        u8g2.setCursor(4, y);
        u8g2.print(label);
        int16_t vw = u8g2.getUTF8Width(val);
        u8g2.setCursor(OLED_WIDTH - vw - 6, y);   // -6: clear the scrollbar lane
        u8g2.print(val);
    }
    u8g2.setForegroundColor(WHITE);

    // Right-edge scrollbar: track + proportional thumb (position in the list).
    const int16_t trackY = 11, trackH = 43;       // content band
    oled.drawFastVLine(OLED_WIDTH - 1, trackY, trackH, WHITE);
    int16_t thumbH = trackH * VISIBLE / ITEM_COUNT;
    if (thumbH < 4) thumbH = 4;
    int16_t thumbY = trackY + (trackH - thumbH) * scrollTop /
                     (ITEM_COUNT - VISIBLE);
    oled.fillRect(OLED_WIDTH - 3, thumbY, 3, thumbH, WHITE);

    UI::footer("K3:next", "K4:change");
}

void ScreenSettings::renderCalib() {
    UI::header("CALIBRATE");
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);

    u8g2.setFont(u8g2_font_6x10_tf);
    const char* l1 = "Rotate slowly";
    const char* l2 = "in a figure-8";
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(l1)), 24); u8g2.print(l1);
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(l2)), 35); u8g2.print(l2);

    // Progress bar (time elapsed of the calibration window).
    uint32_t elapsed = Clock::uptimeMs() - calibStartMs;
    uint16_t pct = (uint16_t)(elapsed * 100 / CALIB_DURATION_MS);
    if (pct > 100) pct = 100;
    oled.drawRect(2, 39, 124, 6, WHITE);
    oled.fillRect(2, 39, (uint16_t)(pct * 124UL / 100), 6, WHITE);

    char info[8];
    snprintf(info, sizeof(info), "%u%%", pct);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(info)), 53);
    u8g2.print(info);

    UI::footer("K3:cancel", "K4:save");
}

void ScreenSettings::render() {
    if (mode == Mode::CALIBRATING) renderCalib();
    else                           renderList();
}
