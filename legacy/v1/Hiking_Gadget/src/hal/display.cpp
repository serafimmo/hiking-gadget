#include "display.h"
#include "../config.h"

Adafruit_SSD1306   oled(OLED_WIDTH, OLED_HEIGHT, &Wire1, -1);
U8G2_FOR_ADAFRUIT_GFX u8g2;

static bool _sleeping = false;
static bool _present  = false;

bool Display::begin() {
    // Don't halt if the panel is missing — keep running so Serial diagnostics
    // still report the I2C scan and sensor telemetry (vital for bring-up).
    _present = oled.begin(SSD1306_SWITCHCAPVCC, ADDR_SSD1306);
    if (!_present) return false;
    u8g2.begin(oled);
    oled.setTextWrap(false);
    oled.clearDisplay();
    oled.display();
    return true;
}

bool Display::present() { return _present; }

void Display::sleep() {
    if (!_present) return;
    oled.ssd1306_command(SSD1306_DISPLAYOFF);
    _sleeping = true;
}

void Display::wake() {
    if (!_present) return;
    oled.ssd1306_command(SSD1306_DISPLAYON);
    _sleeping = false;
}

bool Display::isSleeping() { return _sleeping; }

void Display::clear() { if (_present) oled.clearDisplay(); }
void Display::show()  { if (_present) oled.display(); }

void Display::invertFlash(uint8_t times, uint16_t periodMs) {
    for (uint8_t i = 0; i < times * 2; i++) {
        oled.invertDisplay(i & 1);
        delay(periodMs / 2);
    }
    oled.invertDisplay(false);
}
