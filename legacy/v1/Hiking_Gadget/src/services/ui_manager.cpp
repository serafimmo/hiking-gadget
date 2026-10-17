#include "ui_manager.h"
#include "settings_store.h"
#include "../config.h"
#include "../hal/buttons.h"
#include "../hal/display.h"
#include "../hal/ui.h"
#include "../hal/battery.h"
#include "../hal/power.h"
#include "clock.h"
#include "log.h"
#include "../screens/screen_base.h"
#include <esp_sleep.h>
#include "driver/gpio.h"
#include "../screens/screen_home.h"
#include "../screens/screen_stopwatch.h"
#include "../screens/screen_timer.h"
#include "../screens/screen_compass.h"
#include "../screens/screen_settings.h"

static ScreenHome      scr_home;
static ScreenStopwatch scr_stopwatch;
static ScreenTimer     scr_timer;
static ScreenCompass   scr_compass;
static ScreenSettings  scr_settings;

static ScreenBase* screens[(uint8_t)ScreenID::COUNT] = {
    &scr_home,
    &scr_stopwatch,
    &scr_timer,
    &scr_compass,
    &scr_settings,
};

static uint8_t  curIdx       = 0;
static uint32_t lastActivity = 0;
static bool     justWoke     = false;

static inline ScreenBase* cur() { return screens[curIdx]; }

static const char* screenName(uint8_t i) {
    static const char* N[] = {"HOME","STOPWATCH","TIMER","COMPASS","SETTINGS"};
    return (i < (uint8_t)ScreenID::COUNT) ? N[i] : "?";
}
static const char* eventName(ButtonEvent ev) {
    switch (ev) {
        case ButtonEvent::K1_SHORT: return "K1";
        case ButtonEvent::K2_SHORT: return "K2";
        case ButtonEvent::K3_SHORT: return "K3";
        case ButtonEvent::K4_SHORT: return "K4";
        case ButtonEvent::K1_LONG:  return "K1_LONG";
        case ButtonEvent::K2_LONG:  return "K2_LONG";
        default:                    return "?";
    }
}

static void goTo(uint8_t newIdx) {
    cur()->onExit();
    curIdx = newIdx;
    cur()->onEnter();
    LOGI("NAV", "-> %s", screenName(curIdx));
}

// Top-bar overlay drawn on every screen: page dots (centre) + battery (right).
// Lives in the top 8px strip so it never collides with screen content/footers.
static void drawTopOverlay() {
    const uint8_t N   = (uint8_t)ScreenID::COUNT; // 5
    const uint8_t DOT = 3;  // active dot width
    const uint8_t GAP = 3;  // spacing between dot centres extras
    // Page dots: active = filled 3px bar, inactive = single pixel
    uint8_t totalW = (N - 1) * (DOT + GAP) + DOT;
    uint8_t sx = (OLED_WIDTH - totalW) / 2;
    uint8_t y  = 3;
    for (uint8_t i = 0; i < N; i++) {
        uint8_t x = sx + i * (DOT + GAP);
        if (i == curIdx) {
            oled.fillRect(x, y - 1, DOT, 3, WHITE);   // bold marker
        } else {
            oled.drawPixel(x + 1, y, WHITE);          // faint dot
        }
    }

    // Battery (top-right) — only if a pack is detected
    uint8_t pct = Battery::percent();
    if (pct != 255) {
        UI::batteryIcon(OLED_WIDTH - 15, 0, pct);
    }
}

void UIManager::begin() {
    curIdx       = 0;
    lastActivity = Clock::uptimeMs();
    cur()->onEnter();
}

ScreenID UIManager::currentScreen() { return (ScreenID)curIdx; }

void UIManager::tick() {
    uint32_t now = Clock::uptimeMs();   // RTC-backed: keeps advancing in sleep
    const uint8_t N = (uint8_t)ScreenID::COUNT;

    // ── Headless mode (no OLED): keep logic + Serial logs alive, no sleep ──
    if (!Display::present()) {
        while (Buttons::hasEvent()) {
            ButtonEvent ev = Buttons::dequeue();
            LOGI("BTN", "%s scr=%s", eventName(ev), screenName(curIdx));
            if      (ev == ButtonEvent::K1_SHORT) goTo((curIdx + N - 1) % N);
            else if (ev == ButtonEvent::K2_SHORT) goTo((curIdx + 1) % N);
            else                                  cur()->onButton(ev);
        }
        cur()->tick();
        return;
    }

    // ── Display timeout ───────────────────────────────────────────
    uint32_t timeoutMs = (uint32_t)gSettings.displayTimeoutSec * 1000UL;
    if (timeoutMs > 0 && !Display::isSleeping()) {
        if ((now - lastActivity) >= timeoutMs) {
            Display::sleep();
            LOGI("DISP", "off (idle timeout)");
        }
    }

    // ── Button processing ─────────────────────────────────────────
    while (Buttons::hasEvent()) {
        ButtonEvent ev = Buttons::dequeue();

        lastActivity = now;

        if (Display::isSleeping()) {
            Display::wake();
            cur()->markDirty();
            justWoke = true;
            LOGI("DISP", "on (button wake)");
            continue; // swallow event that woke the display
        }
        if (justWoke) {
            justWoke = false;
            continue; // swallow one more event after wake
        }

        LOGI("BTN", "%s scr=%s", eventName(ev), screenName(curIdx));

        // Navigation (K1=prev, K2=next)
        if (ev == ButtonEvent::K1_SHORT) {
            goTo((curIdx + N - 1) % N);
        } else if (ev == ButtonEvent::K2_SHORT) {
            goTo((curIdx + 1) % N);
        } else {
            cur()->onButton(ev);
        }
    }
    justWoke = false;

    // ── Tick active screen (runs even while the screen is off, so a timer
    //    keeps counting in the background and can wake the display) ─────
    cur()->tick();
    if (Display::isSleeping() && cur()->wantsWake()) {
        Display::wake();
        lastActivity = now;
        cur()->markDirty();
        LOGI("DISP", "on (%s alarm)", screenName(curIdx));
    }

    // ── Render if dirty ───────────────────────────────────────────
    if (!Display::isSleeping() && cur()->isDirty()) {
        Display::clear();
        cur()->render();
        drawTopOverlay();  // page dots + battery, consistent across all screens
        Display::show();
        cur()->clearDirty();
    }

    // ── Deep sleep after a long idle (full power-down, ~10uA) ─────────
    // Skipped while a stopwatch/timer is running so we never lose a count.
    if (Display::isSleeping() && !cur()->isBusy() &&
        (now - lastActivity) >= (uint32_t)AUTO_DEEP_SLEEP_SEC * 1000UL) {
        LOGI("PWR", "deep sleep (idle %lus)", (unsigned long)AUTO_DEEP_SLEEP_SEC);
        if (Serial) Serial.flush();   // only when a USB host is attached (no stall on battery)
        Power::deepSleep();   // does not return; wakes via setup()
    }

    // ── Light sleep while the screen is off (saves the small battery) ──
    // RTC keeps running, so the clock and stopwatch/timer stay correct.
    // Wakes on any button (active-low) or after a short slice.
    if (Display::isSleeping() && gSettings.lowPower) {
        gpio_wakeup_enable((gpio_num_t)PIN_K1, GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable((gpio_num_t)PIN_K2, GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable((gpio_num_t)PIN_K3, GPIO_INTR_LOW_LEVEL);
        gpio_wakeup_enable((gpio_num_t)PIN_K4, GPIO_INTR_LOW_LEVEL);
        esp_sleep_enable_gpio_wakeup();
        esp_sleep_enable_timer_wakeup((uint64_t)IDLE_SLEEP_MS * 1000ULL);
        esp_light_sleep_start();
    }
}
