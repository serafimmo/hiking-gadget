#pragma once
// Chrome components (SPEC §3.4): status line with the chip budget (D30), Home top line, rail, tag, toast, battery,
// progress bar, card header.
#include "bindings.h"
#include "canvas.h"
#include "view_model.h"

namespace hg {
struct ChipSet { bool warn, bell, tmr, sw; int32_t tmrRemainS; };
ChipSet chipsFor(const BgChips& bg, ScreenId page);    // a page never shows the chip of its own model (§2.3.5)

namespace widgets {
void rule(Canvas& c, int16_t y);                                             // dotted, even x 0..116
int16_t tagWidth(Canvas& c, const char* word);                               // box width = text + 3
int16_t tag(Canvas& c, int16_t x, int16_t y, const char* word);              // returns the box width
// Chips right-to-left ending at xRight (inclusive), never left of L; returns the x of the leftmost chip drawn
// (xRight + 1 if none). iconY / textInk: 1 on status lines, 2 on Home.
int16_t chips(Canvas& c, int16_t xRight, int16_t L, int16_t iconY, int16_t textInk, const ChipSet& s);
int16_t chipsWidth(Canvas& c, const ChipSet& s);                             // all chips + gaps
// Status line: title (Micro caps) at (0,1), optional tag after it, clock R@116 (nullptr = none), chips, rule y 8.
void statusLine(Canvas& c, const char* title, const char* tagWord, const char* clock, const ChipSet* chipSet);
// Home: date (or "Clock not set") at (0,1), zone R@116, chips and SYNC? between them with the §3.4 overflow steps.
void homeTopLine(Canvas& c, const Civil& local, bool clockValid, const char* zone, const ChipSet& chipSet, bool syncTag);
void battery(Canvas& c, int16_t x, int16_t y, const BatteryState& b, bool blinkOn);
void progressBar(Canvas& c, int16_t y, float p);
void cardHeader(Canvas& c, const char* title);
void toast(Canvas& c, const char* text, int16_t dy);                        // dy 0 = fully shown, 15 = hidden below
struct RailState {
  bool dots;               // page dots (carousel) unless the bindings ask for chevrons
  uint8_t page, pageCount;
  uint8_t fromPage; float slideT;   // dot morph during a slide (slideT 1 = done)
  uint8_t pressedMask;     // keycaps drawn pressed (Down until Up, >= 80 ms)
  HoldState hold;          // hold fill of the holding key
  bool flash;              // the hold just completed: cap filled for 80 ms
};
void rail(Canvas& c, const Bindings& b, const RailState& r);
}  // namespace widgets
}  // namespace hg
