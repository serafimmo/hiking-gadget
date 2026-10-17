#include "ui.h"
#include "display.h"
#include "../config.h"

int16_t UI::centerX(int16_t w) {
    int16_t x = (OLED_WIDTH - w) / 2;
    return (x < 0) ? 0 : x;
}

void UI::header(const char* title) {
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.setCursor(0, TOP_BAR_Y);
    u8g2.print(title);
    oled.drawFastHLine(0, TOP_SEP_Y, OLED_WIDTH, WHITE);
}

void UI::footer(const char* k1, const char* k2) {
    oled.drawFastHLine(0, FOOT_SEP_Y, OLED_WIDTH, WHITE);
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);
    u8g2.setFont(u8g2_font_5x7_tf);
    if (k1) { u8g2.setCursor(2,  FOOT_TEXT_Y); u8g2.print(k1); }
    if (k2) { u8g2.setCursor(66, FOOT_TEXT_Y); u8g2.print(k2); }
}

void UI::footerCentered(const char* text) {
    oled.drawFastHLine(0, FOOT_SEP_Y, OLED_WIDTH, WHITE);
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);
    u8g2.setFont(u8g2_font_5x7_tf);
    int16_t w = u8g2.getUTF8Width(text);
    u8g2.setCursor(centerX(w), FOOT_TEXT_Y);
    u8g2.print(text);
}

void UI::batteryIcon(int16_t x, int16_t y, uint8_t pct) {
    if (pct > 100) pct = 100;
    // Body 12x6, terminal bump 2x3
    oled.drawRect(x, y, 12, 6, WHITE);
    oled.fillRect(x + 12, y + 2, 2, 2, WHITE);
    // Inner fill proportional to charge (max 10px wide)
    uint8_t fillW = (uint8_t)((pct * 10UL + 50) / 100);
    if (fillW > 0) oled.fillRect(x + 1, y + 1, fillW, 4, WHITE);
}

// ── Tiny glyphs (5px wide, top-left origin) ───────────────────────────────────
void UI::iconThermo(int16_t x, int16_t y) {
    // Outlined tube with rounded top, a mercury column and a round bulb.
    oled.drawPixel(x + 2, y, WHITE);            // rounded top
    oled.drawFastVLine(x + 1, y + 1, 5, WHITE); // left wall
    oled.drawFastVLine(x + 3, y + 1, 5, WHITE); // right wall
    oled.fillRect(x + 2, y + 3, 1, 5, WHITE);   // mercury column
    oled.fillCircle(x + 2, y + 8, 2, WHITE);    // bulb
}

void UI::iconDrop(int16_t x, int16_t y) {
    // Solid teardrop: pointed top tapering into a round bottom.
    oled.fillTriangle(x + 2, y, x, y + 4, x + 4, y + 4, WHITE);
    oled.fillCircle(x + 2, y + 5, 2, WHITE);
}

void UI::iconWifi(int16_t x, int16_t y) {
    // Three rising arcs + base dot
    oled.drawPixel(x + 2, y + 6, WHITE);
    oled.drawFastHLine(x + 1, y + 4, 3, WHITE);
    oled.drawFastHLine(x,     y + 2, 5, WHITE);
    oled.drawPixel(x, y + 1, WHITE);
    oled.drawPixel(x + 4, y + 1, WHITE);
}
