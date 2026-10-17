#include "screen_compass.h"
#include "../hal/display.h"
#include "../drivers/compass_driver.h"
#include <math.h>
#include <stdio.h>

// Compass rose: center (90, 31), radius 26
// Left panel (0..60): bearing degrees + cardinal name
#define CX  90
#define CY  31
#define CR  26

const char* ScreenCompass::cardinalName(float deg) {
    uint8_t idx = (uint8_t)((deg + 22.5f) / 45.0f) % 8;
    static const char* NAMES[] = {"N","NE","E","SE","S","SW","W","NW"};
    return NAMES[idx];
}

void ScreenCompass::onEnter() { _dirty = true; }

void ScreenCompass::onButton(ButtonEvent ev) {
    CompassData cd = CompassDriver::getData();
    if (ev == ButtonEvent::K3_SHORT && cd.valid) {
        targetBrg = (int16_t)(cd.angle + 0.5f) % 360;   // hold current heading
        _dirty = true;
    } else if (ev == ButtonEvent::K4_SHORT) {
        targetBrg = -1;                                 // clear target
        _dirty = true;
    }
}

void ScreenCompass::tick() {
    if (CompassDriver::angleChanged()) _dirty = true;
}

// Marker on the rim at an absolute bearing (same projection as the rose).
void ScreenCompass::drawTargetMark(float bearingDeg) {
    float rad = bearingDeg * M_PI / 180.0f;
    int16_t mx = CX + (int16_t)((CR + 1) * sinf(rad));
    int16_t my = CY - (int16_t)((CR + 1) * cosf(rad));
    oled.fillCircle(mx, my, 2, WHITE);
    oled.drawCircle(mx, my, 3, WHITE);   // halo so it stands out on the ring
}

// A single fine bezel ring with graduated ticks — cardinals long, the rest
// short — and the four cardinal letters set well inside with clear air.
void ScreenCompass::drawCardinal(const char* s, int16_t px, int16_t py) {
    int16_t w = u8g2.getUTF8Width(s);
    u8g2.setCursor(px - w / 2, py + 3);   // centre the glyph on (px, py)
    u8g2.print(s);
}

void ScreenCompass::drawRose() {
    oled.drawCircle(CX, CY, CR, WHITE);

    // 12 graduated ticks (every 30°); the four cardinals are longer.
    for (uint8_t i = 0; i < 12; i++) {
        float rad = i * (M_PI / 6.0f);
        int16_t inner = (i % 3 == 0) ? (CR - 4) : (CR - 2);
        int16_t x1 = CX + (int16_t)(inner * sinf(rad));
        int16_t y1 = CY - (int16_t)(inner * cosf(rad));
        int16_t x2 = CX + (int16_t)((CR - 1) * sinf(rad));
        int16_t y2 = CY - (int16_t)((CR - 1) * cosf(rad));
        oled.drawLine(x1, y1, x2, y2, WHITE);
    }

    // Cardinal letters, set inward (radius CR-12) with clear space from ticks.
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);
    u8g2.setFont(u8g2_font_5x7_tf);
    const int16_t LR = CR - 12;
    drawCardinal("N", CX,      CY - LR);
    drawCardinal("S", CX,      CY + LR);
    drawCardinal("E", CX + LR, CY);
    drawCardinal("W", CX - LR, CY);
}

void ScreenCompass::drawNeedle(float angleDeg) {
    float rad  = angleDeg * M_PI / 180.0f;
    float s    = sinf(rad), c = cosf(rad);
    float ps   = sinf(rad + M_PI / 2.0f), pc = cosf(rad + M_PI / 2.0f);
    int16_t Rn = CR - 8;    // north arrow length
    int16_t Rs = CR - 14;   // south tail length
    int16_t bw = 2;         // arrow base half-width

    int16_t tipX  = CX + (int16_t)(Rn * s);
    int16_t tipY  = CY - (int16_t)(Rn * c);
    int16_t bxA   = CX + (int16_t)(bw * ps), byA = CY - (int16_t)(bw * pc);
    int16_t bxB   = CX - (int16_t)(bw * ps), byB = CY + (int16_t)(bw * pc);
    int16_t tailX = CX - (int16_t)(Rs * s),  tailY = CY + (int16_t)(Rs * c);

    // North end: a solid arrowhead. South end: a thin tail.
    oled.fillTriangle(tipX, tipY, bxA, byA, bxB, byB, WHITE);
    oled.drawLine(CX, CY, tailX, tailY, WHITE);
    oled.fillCircle(CX, CY, 1, WHITE);
}

void ScreenCompass::render() {
    CompassData cd = CompassDriver::getData();

    // ── Rose ──────────────────────────────────────────────────────
    drawRose();
    if (targetBrg >= 0) drawTargetMark((float)targetBrg);

    if (cd.valid) {
        drawNeedle(cd.angle);
    } else {
        // "?" in center when no data
        u8g2.setFont(u8g2_font_7x14B_tf);
        u8g2.setFontMode(1);
        u8g2.setForegroundColor(WHITE);
        u8g2.setCursor(CX - 3, CY + 5);
        u8g2.print("?");
    }

    // ── Left info column (no divider — clean whitespace separation) ──
    u8g2.setFontMode(1);
    u8g2.setForegroundColor(WHITE);

    if (cd.valid) {
        // Bearing (large digits, left-aligned) + drawn degree ring
        char degBuf[5];
        snprintf(degBuf, sizeof(degBuf), "%03.0f", cd.angle);   // leading zeros: 045
        u8g2.setFont(u8g2_font_logisoso16_tn);
        u8g2.setCursor(2, 22);
        u8g2.print(degBuf);
        int16_t nx = 2 + u8g2.getUTF8Width(degBuf);
        oled.drawCircle(nx + 3, 9, 1, WHITE);    // degree symbol, top-right

        // Cardinal direction (left-aligned, same column)
        u8g2.setFont(u8g2_font_7x14B_tf);
        u8g2.setCursor(2, 41);
        u8g2.print(cardinalName(cd.angle));
    } else {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setCursor(2, 22);
        u8g2.print("---");
        u8g2.setFont(u8g2_font_7x14B_tf);
        u8g2.setCursor(2, 41);
        u8g2.print("--");
    }

    // Target bearing readout, or hint to set one
    u8g2.setFont(u8g2_font_5x7_tf);
    if (targetBrg >= 0) {
        char tb[12];
        snprintf(tb, sizeof(tb), "tgt %03d", targetBrg);
        u8g2.setCursor(2, 55);
        u8g2.print(tb);
        u8g2.setCursor(2, 63);
        u8g2.print("K4 clear");
    } else {
        u8g2.setCursor(2, 55);
        u8g2.print("K3 set");
        u8g2.setCursor(2, 63);
        u8g2.print("target");
    }
}
