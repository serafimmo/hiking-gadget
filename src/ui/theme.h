#pragma once
// Screen grid (SPEC §3.2). Font roles -> u8g2 fonts (§3.3) are mapped in canvas.cpp:
//   Hero logisoso30_tn, Big logisoso28_tn, BigS logisoso24_tn, Heading logisoso22_tn, Mid logisoso16_tr,
//   Label helvB08_tr, Value helvB10_tr, Body haxrcorp4089_tr, Micro tom_thumb_4x6_mr.
#include "../core/types.h"
namespace hg {
constexpr int16_t kW = 128, kH = 64;
constexpr int16_t kContentX1 = 116;                    // content x 0..116 (117 px); x 117..118 always empty
constexpr int16_t kRailX = 119, kRailW = 9;            // rail x 119..127
constexpr int16_t kStatusInk = 1, kStatusRuleY = 8, kBodyTop = 10;
constexpr int16_t kDotsX = 122, kDotsTop = 1;          // active bar 3x6 at x 122..124 (advance 8); inactive 2x2 at x 123..124 (advance 4)
constexpr int16_t kChevX = 121, kChevUpY = 1, kChevDnY = 15;
constexpr int16_t kCapHashY = 42, kCapStarY = 54, kCapH = 10, kCapW = 9;
constexpr int16_t kCapIconX = 121, kCapIconDy = 1;
constexpr int16_t kLabelRight = 116, kLabelInkDy = 2;
constexpr int16_t kRailSlotY[4] = {1, 15, 42, 54};     // Diagnostics > Buttons boxes 9x9 next to ^ v # *
constexpr int16_t kKeyRowEstimate[4] = {-19, 16, 48, 80};   // documentation only; tuned on hardware [HW?]
constexpr int16_t kToastMaxTextW = 105;
}  // namespace hg
