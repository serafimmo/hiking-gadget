#pragma once
#include <stdint.h>

// ── Shared UI chrome ──────────────────────────────────────────────────────────
// Consistent layout grid used by every screen (128x64):
//   Top bar    y=0..8   : screen title (left) · page dots (centre) · battery (right)
//   Separator  y=9
//   Content    y=11..53 : screen-specific
//   Separator  y=54
//   Footer     y=56..63 : K1 / K2 action hints
//
// Top bar (dots + battery) is drawn by UIManager as an overlay, so screens only
// draw their title + content + footer.

namespace UI {
    // Layout anchors — single source of truth for every screen
    static const uint8_t TOP_BAR_Y     = 7;   // title baseline
    static const uint8_t TOP_SEP_Y     = 9;   // separator under top bar
    static const uint8_t CONTENT_TOP   = 11;
    static const uint8_t CONTENT_BOT   = 53;
    static const uint8_t FOOT_SEP_Y    = 54;  // separator above footer
    static const uint8_t FOOT_TEXT_Y   = 63;  // footer baseline

    // Draw screen title (top-left, 5x7) + separator line under the top bar.
    void header(const char* title);

    // Draw footer separator + up to two action hints.
    // Pass nullptr to omit a hint.
    void footer(const char* k1, const char* k2);

    // One centred footer hint (e.g. "K1/K2: dismiss").
    void footerCentered(const char* text);

    // Small battery glyph at (x, y); pct 0..100. Drawn as outline + fill.
    void batteryIcon(int16_t x, int16_t y, uint8_t pct);

    // Tiny 7px-tall glyphs (top-left origin) for labelling sensor readouts.
    void iconThermo(int16_t x, int16_t y);   // thermometer
    void iconDrop(int16_t x, int16_t y);     // humidity droplet
    void iconWifi(int16_t x, int16_t y);     // wifi arcs

    // Centre helper: x so that a string of pixel-width w is centred in [0,128).
    int16_t centerX(int16_t w);
}
