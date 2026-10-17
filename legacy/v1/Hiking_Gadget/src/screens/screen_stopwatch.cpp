#include "screen_stopwatch.h"
#include "../hal/display.h"
#include "../hal/ui.h"
#include "../services/clock.h"
#include <stdio.h>

// Use RTC-backed monotonic time so the stopwatch keeps counting through sleep.
#define NOW_MS() Clock::uptimeMs()

void ScreenStopwatch::onEnter() { _dirty = true; }

uint32_t ScreenStopwatch::elapsedMs() const {
    if (state == State::RUNNING)
        return accumulatedMs + (NOW_MS() - startMs);
    return accumulatedMs;
}

void ScreenStopwatch::formatTime(char* buf, size_t len, uint32_t ms) const {
    uint32_t mins = ms / 60000;
    uint32_t secs = (ms % 60000) / 1000;
    uint32_t cent = (ms % 1000) / 10;
    snprintf(buf, len, "%02u:%02u.%02u",
             (unsigned)mins, (unsigned)secs, (unsigned)cent);
}

void ScreenStopwatch::onButton(ButtonEvent ev) {
    if (ev == ButtonEvent::K3_SHORT) {
        // Start / Stop
        if (state == State::IDLE || state == State::PAUSED) {
            startMs = NOW_MS();
            state   = State::RUNNING;
        } else {
            accumulatedMs = elapsedMs();
            state         = State::PAUSED;
        }
        _dirty = true;
    } else if (ev == ButtonEvent::K4_SHORT) {
        if (state == State::RUNNING) {
            // Record lap
            if (lapCount < MAX_LAPS) {
                laps[lapCount++] = elapsedMs();
            }
        } else if (state == State::PAUSED || state == State::IDLE) {
            // Reset
            state         = State::IDLE;
            accumulatedMs = 0;
            lapCount      = 0;
            prevElapsed   = 0;
        }
        _dirty = true;
    }
}

void ScreenStopwatch::tick() {
    if (state == State::RUNNING) {
        uint32_t el = elapsedMs();
        // Refresh every 50ms (hundredths of seconds)
        if ((el / 50) != (prevElapsed / 50)) {
            prevElapsed = el;
            _dirty = true;
        }
    }
}

void ScreenStopwatch::render() {
    uint32_t el = elapsedMs();
    char mainBuf[12];
    formatTime(mainBuf, sizeof(mainBuf), el);

    UI::header("STOPWATCH");

    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);

    // ── Main time (large, centred) ────────────────────────────────
    u8g2.setFont(u8g2_font_logisoso16_tn);
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(mainBuf)), 27);
    u8g2.print(mainBuf);

    // ── State label (centred under time) ──────────────────────────
    const char* st = (state == State::RUNNING) ? "RUNNING" :
                     (state == State::PAUSED)  ? "PAUSED"  : "READY";
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(st)), 37);
    u8g2.print(st);

    // ── Lap list: last two, most recent first, time right-aligned ─────────
    uint8_t shown = (lapCount < 2) ? lapCount : 2;
    for (uint8_t i = 0; i < shown; i++) {
        uint8_t idx = lapCount - 1 - i;
        char timeTmp[12], lab[8];
        formatTime(timeTmp, sizeof(timeTmp), laps[idx]);
        snprintf(lab, sizeof(lab), "Lap %u", (unsigned)(idx + 1));
        int16_t y = 45 + i * 8;        // baselines 45, 53 — clear of footer (54)
        u8g2.setCursor(4, y);
        u8g2.print(lab);
        int16_t tw = u8g2.getUTF8Width(timeTmp);
        u8g2.setCursor(OLED_WIDTH - tw - 4, y);
        u8g2.print(timeTmp);
    }

    // ── Footer hints ──────────────────────────────────────────────
    if (state == State::RUNNING)      UI::footer("K3:STOP",   "K4:LAP");
    else if (state == State::PAUSED)  UI::footer("K3:RESUME", "K4:RESET");
    else                              UI::footer("K3:START",  "K4:RESET");
}
