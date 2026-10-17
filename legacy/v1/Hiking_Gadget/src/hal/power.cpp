#include "power.h"
#include "display.h"
#include "ui.h"
#include "../config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include "driver/rtc_io.h"

void Power::begin() {
    setCpuFrequencyMhz(CPU_FREQ_MHZ);
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF);
}

bool Power::wokeFromDeepSleep() {
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1;
}

void Power::deepSleep() {
    // Farewell frame
    Display::wake();
    Display::clear();
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);
    u8g2.setFont(u8g2_font_7x14B_tf);
    const char* m = "Power off";
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(m)), 30);
    u8g2.print(m);
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* h = "press any key to wake";
    u8g2.setCursor(UI::centerX(u8g2.getUTF8Width(h)), 46);
    u8g2.print(h);
    Display::show();
    delay(900);
    Display::sleep();          // turn the panel off to save power in sleep

    // Configure ext1 wake on any button (active-low). Enable RTC pull-ups so the
    // idle level is HIGH and we only wake on an actual press.
    const gpio_num_t pins[4] = {
        (gpio_num_t)PIN_K1, (gpio_num_t)PIN_K2,
        (gpio_num_t)PIN_K3, (gpio_num_t)PIN_K4
    };
    uint64_t mask = 0;
    for (uint8_t i = 0; i < 4; i++) {
        rtc_gpio_pullup_en(pins[i]);
        rtc_gpio_pulldown_dis(pins[i]);
        mask |= (1ULL << pins[i]);
    }
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_deep_sleep_start();    // never returns; boot runs setup() on wake
}
