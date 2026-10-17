#include "splash.h"
#include "display.h"
#include "../config.h"
#include <Arduino.h>

// Draw the static splash scene (mountains + title). Progress bar drawn separately.
static void drawScene() {
    oled.clearDisplay();
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);

    // ── Mountain silhouette (hiking theme) ────────────────────────
    // Three peaks on a base line at y=24
    oled.drawLine(6,  24, 26, 4,  WHITE);
    oled.drawLine(26, 4,  44, 24, WHITE);
    oled.drawLine(34, 24, 64, 2,  WHITE);
    oled.drawLine(64, 2,  92, 24, WHITE);
    oled.drawLine(80, 24, 102, 8, WHITE);
    oled.drawLine(102, 8, 122, 24, WHITE);
    oled.drawFastHLine(2, 24, 124, WHITE);
    // Snow caps (short marks near the two tallest peaks)
    oled.drawLine(60, 8,  64, 4,  WHITE);
    oled.drawLine(64, 4,  68, 8,  WHITE);
    oled.drawLine(23, 9,  26, 6,  WHITE);
    oled.drawLine(26, 6,  29, 9,  WHITE);

    // ── Title ─────────────────────────────────────────────────────
    u8g2.setFont(u8g2_font_7x14B_tf);
    const char* title = "HIKE GADGET";
    int16_t tw = u8g2.getUTF8Width(title);
    u8g2.setCursor((OLED_WIDTH - tw) / 2, 40);
    u8g2.print(title);

    // ── Subtitle ──────────────────────────────────────────────────
    u8g2.setFont(u8g2_font_5x7_tf);
    const char* sub = "v1.0  ESP32-S3";
    int16_t sw = u8g2.getUTF8Width(sub);
    u8g2.setCursor((OLED_WIDTH - sw) / 2, 50);
    u8g2.print(sub);

    // Progress bar frame
    oled.drawRect(2, 56, 124, 6, WHITE);
}

void Splash::show(uint16_t durationMs) {
    drawScene();
    oled.display();

    // Animated progress fill — gives a polished "booting" feel
    uint32_t start = millis();
    uint16_t lastW = 0;
    while (true) {
        uint32_t elapsed = millis() - start;
        if (elapsed >= durationMs) break;
        uint16_t w = (uint16_t)((uint32_t)elapsed * 122 / durationMs);
        if (w != lastW) {
            oled.fillRect(3, 57, w, 4, WHITE);
            oled.display();
            lastW = w;
        }
        delay(20);
    }
    // Ensure bar is full at the end
    oled.fillRect(3, 57, 122, 4, WHITE);
    oled.display();
    delay(120);
}
