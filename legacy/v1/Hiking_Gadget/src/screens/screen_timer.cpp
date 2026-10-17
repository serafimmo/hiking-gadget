#include "screen_timer.h"
#include "../hal/display.h"
#include "../hal/ui.h"
#include "../services/settings_store.h"
#include "../services/clock.h"
#include <stdio.h>

// RTC-backed monotonic time so the countdown survives light sleep.
#define NOW_MS() Clock::uptimeMs()

const uint16_t ScreenTimer::PRESETS[]      = {1, 3, 5, 10, 30, 60}; // minutes
const uint8_t  ScreenTimer::PRESET_COUNT   = 6;

void ScreenTimer::onEnter() {
    // Restore last-used preset from settings
    presetIdx = gSettings.timerPresetIndex % PRESET_COUNT;
    _dirty    = true;
}

void ScreenTimer::onExit() {
    gSettings.timerPresetIndex = presetIdx;
    SettingsStore::save();
}

uint32_t ScreenTimer::remainingMs() const {
    if (state != State::RUNNING) return durationMs;
    uint32_t elapsed = NOW_MS() - startMs;
    return (elapsed >= durationMs) ? 0 : durationMs - elapsed;
}

void ScreenTimer::formatRemaining(char* buf, size_t len) const {
    uint32_t ms   = remainingMs();
    uint32_t mins = ms / 60000;
    uint32_t secs = (ms % 60000) / 1000;
    snprintf(buf, len, "%02u:%02u", (unsigned)mins, (unsigned)secs);
}

void ScreenTimer::onButton(ButtonEvent ev) {
    if (state == State::SELECT) {
        if (ev == ButtonEvent::K3_SHORT) {
            presetIdx = (presetIdx + 1) % PRESET_COUNT;
            _dirty = true;
        } else if (ev == ButtonEvent::K4_SHORT) {
            durationMs = (uint32_t)PRESETS[presetIdx] * 60000UL;
            startMs    = NOW_MS();
            state      = State::RUNNING;
            prevRemain = durationMs;
            _dirty     = true;
        }
    } else if (state == State::RUNNING) {
        if (ev == ButtonEvent::K4_SHORT) {
            // Stop and return to selection
            state  = State::SELECT;
            _dirty = true;
        }
    } else if (state == State::ALERT) {
        if (ev == ButtonEvent::K3_SHORT || ev == ButtonEvent::K4_SHORT) {
            state  = State::SELECT;
            _dirty = true;
        }
    }
}

void ScreenTimer::tick() {
    if (state == State::RUNNING) {
        uint32_t rem = remainingMs();
        if (rem == 0) {
            state       = State::ALERT;
            alertStart  = NOW_MS();
            wakeRequest = true;   // ask UIManager to wake the screen if it's off
            _dirty      = true;
            return;
        }
        if ((rem / 1000) != (prevRemain / 1000)) {
            prevRemain = rem;
            _dirty     = true;
        }
    }
    if (state == State::ALERT) {
        // Flash every 400ms for 10 seconds
        uint32_t elapsed = NOW_MS() - alertStart;
        bool flip = (elapsed / 400) % 2;
        if (flip != alertFlip) {
            alertFlip = flip;
            _dirty    = true;
        }
        if (elapsed > 10000) {
            state  = State::SELECT;
            _dirty = true;
        }
    }
}

bool ScreenTimer::wantsWake() {
    if (wakeRequest) { wakeRequest = false; return true; }
    return false;
}

void ScreenTimer::render() {
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);

    if (state == State::SELECT) {
        UI::header("TIMER");

        // 6 presets in a 3x2 grid, selected one highlighted
        const uint8_t COL_X[] = {6, 48, 90};
        const uint8_t ROW_Y[] = {30, 48};
        u8g2.setFont(u8g2_font_6x10_tf);
        for (uint8_t i = 0; i < PRESET_COUNT; i++) {
            uint8_t col = i % 3, row = i / 3;
            char buf[8];
            snprintf(buf, sizeof(buf), "%dmin", PRESETS[i]);
            int16_t bx = COL_X[col], by = ROW_Y[row];
            if (i == presetIdx) {
                int16_t tw = u8g2.getUTF8Width(buf);
                oled.fillRect(bx - 3, by - 9, tw + 6, 12, WHITE);
                u8g2.setForegroundColor(BLACK);
                u8g2.setCursor(bx, by);
                u8g2.print(buf);
                u8g2.setForegroundColor(WHITE);
            } else {
                u8g2.setCursor(bx, by);
                u8g2.print(buf);
            }
        }
        UI::footer("K3:NEXT", "K4:START");

    } else if (state == State::RUNNING) {
        UI::header("TIMER");

        char timeBuf[8];
        formatRemaining(timeBuf, sizeof(timeBuf));
        u8g2.setFont(u8g2_font_logisoso24_tn);
        u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(timeBuf)), 40);
        u8g2.print(timeBuf);

        // Progress bar (fills as time elapses)
        uint32_t rem  = remainingMs();
        uint32_t done = (durationMs > rem) ? (durationMs - rem) : 0;
        uint16_t barW = (durationMs > 0)
                        ? (uint16_t)((uint64_t)done * 124 / durationMs) : 0;
        oled.drawRect(2, 45, 124, 6, WHITE);
        if (barW > 0) oled.fillRect(2, 45, barW, 6, WHITE);

        UI::footer("K4:STOP", nullptr);

    } else { // ALERT
        if (alertFlip) {
            oled.fillRect(0, 0, OLED_WIDTH, OLED_HEIGHT, WHITE);
            u8g2.setForegroundColor(BLACK);
        }
        u8g2.setFont(u8g2_font_7x14B_tf);
        const char* msg = "TIME'S UP!";
        u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(msg)), 36);
        u8g2.print(msg);
        u8g2.setFont(u8g2_font_5x7_tf);
        const char* hint = "K3/K4: dismiss";
        u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(hint)), 54);
        u8g2.print(hint);
        u8g2.setForegroundColor(WHITE);
    }
}
