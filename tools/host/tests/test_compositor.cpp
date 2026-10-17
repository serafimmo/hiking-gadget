// ui/compositor vs a reference implementation (SPEC §3.6): d = 0, 1, 7, 8, 63, 64, both directions; rail untouched.
#include <cstring>

#include "tiny_test.h"
#include "ui/compositor.h"

using namespace hg;

namespace {
uint32_t gSeed = 7;
uint8_t rnd() { gSeed = gSeed * 1103515245u + 12345u; return (uint8_t)(gSeed >> 16); }
bool px(const uint8_t* b, int x, int y) { return (b[(y / 8) * 128 + x] >> (y % 8)) & 1; }
}  // namespace

TEST(compositor_matches_reference) {
  uint8_t oldF[1024], newF[1024], live[1024];
  for (int i = 0; i < 1024; ++i) { oldF[i] = rnd(); newF[i] = rnd(); }
  const uint8_t ds[] = {0, 1, 7, 8, 63, 64};
  int bad = 0;
  for (int dir = -1; dir <= 1; dir += 2)
    for (uint8_t d : ds) {
      memcpy(live, newF, sizeof live);
      composeSlide(oldF, live, d, (int8_t)dir, 116);
      for (int x = 0; x < 128; ++x)
        for (int y = 0; y < 64; ++y) {
          bool want;
          if (x > 116) want = px(newF, x, y);                        // rail columns untouched
          else if (dir > 0) want = (y + d < 64) ? px(oldF, x, y + d) : px(newF, x, y + d - 64);   // v: content moves up
          else want = (y - d >= 0) ? px(oldF, x, y - d) : px(newF, x, y - d + 64);              // ^: moves down
          if (px(live, x, y) != want) ++bad;
        }
    }
  CHECK_EQ(bad, 0);
}

TEST(compositor_column_round_trip_and_easing) {
  uint8_t b[1024] = {};
  setFrameColumn(b, 5, 0x8000000000000001ULL);
  CHECK(px(b, 5, 0)); CHECK(px(b, 5, 63)); CHECK(!px(b, 5, 1));
  CHECK_EQ(frameColumn(b, 5), 0x8000000000000001ULL);
  CHECK_EQ(slideOffset(0, 180000), 0);
  CHECK_EQ(slideOffset(180000, 180000), 64);
  CHECK_EQ(slideOffset(90000, 180000), 56);                 // round(64 * (1 - 0.125))
  CHECK_EQ(slideOffset(5, 0), 64);                          // Animations = Off
}
