#pragma once
#include "../config.h"
#include <Adafruit_SSD1306.h>
#include <U8g2_for_Adafruit_GFX.h>

// Global display objects — used by all screen renderers
extern Adafruit_SSD1306   oled;
extern U8G2_FOR_ADAFRUIT_GFX u8g2;

namespace Display {
    bool begin();              // true if the OLED responded at 0x3C
    bool present();            // false → run headless (Serial diagnostics only)
    void sleep();
    void wake();
    bool isSleeping();
    void clear();
    void show();
    void invertFlash(uint8_t times, uint16_t periodMs);
}
