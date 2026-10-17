// core/compass_math (SPEC §6.4, §3.6).
#include <cmath>

#include "core/compass_math.h"
#include "tiny_test.h"

using namespace hg;

namespace {
constexpr float kD2R = 0.017453292519943295f;
// Board-frame horizontal field for a device whose forward edge points to heading psi (deg), mounted as m.
// Device frame: north = (F, L) = (cos psi, sin psi) * H. Invert the mount: F = xc cos t + yc sin t, L = -xc sin t + yc cos t.
void boardField(float psi, const Mount& m, float H, float& bx, float& by) {
  const float F = H * cosf(psi * kD2R), L = H * sinf(psi * kD2R);
  const float t = m.rot * 90.0f * kD2R;
  float xc = F * cosf(t) - L * sinf(t);
  float yc = F * sinf(t) + L * cosf(t);
  if (m.chipDown) yc = -yc;
  bx = xc; by = yc;
}
}  // namespace

TEST(compass_heading_convention) {
  CHECK_NEAR(headingDeg(1.0f, 0.0f), 0.0f, 1e-4f);         // field along +F -> 0
  CHECK_NEAR(headingDeg(0.0f, 1.0f), 90.0f, 1e-4f);        // facing east: north is to the left -> 90
  CHECK_NEAR(headingDeg(-1.0f, 0.0f), 180.0f, 1e-4f);
  CHECK_NEAR(headingDeg(0.0f, -1.0f), 270.0f, 1e-4f);
}

TEST(compass_8_mounts_x_24_headings) {
  MagCal cal{};
  cal.valid = true; cal.scaleX = 1.25f; cal.scaleY = 0.8f; cal.offX = 120; cal.offY = -340;
  int bad = 0;
  for (uint8_t v = 0; v < 8; ++v) {
    const Mount m = mountFromSetting(v);
    CHECK_EQ(m.rot, v & 3); CHECK_EQ(m.chipDown, v >= 4);
    for (int h = 0; h < 24; ++h) {
      const float psi = h * 15.0f;
      float bx, by, F, L;
      boardField(psi, m, 500.0f, bx, by);
      bx = bx / cal.scaleX + cal.offX;                      // apply the hard/soft iron the calibration removes
      by = by / cal.scaleY + cal.offY;
      toDevice(cal, m, bx, by, F, L);
      if (fabsf(wrap180(headingDeg(F, L) - psi)) > 0.01f) ++bad;
    }
  }
  CHECK_EQ(bad, 0);
}

TEST(compass_wrap_point16_hysteresis) {
  CHECK_NEAR(wrap360(-12.0f), 348.0f, 1e-4f);
  CHECK_NEAR(wrap360(372.0f), 12.0f, 1e-4f);
  CHECK_NEAR(wrap360(355.0f + 12.1f), 7.1f, 1e-3f);       // declination wrap
  CHECK_NEAR(wrap180(190.0f), -170.0f, 1e-4f);
  CHECK_NEAR(wrap180(180.0f), 180.0f, 1e-4f);
  CHECK_STR(point16(0), "N");     CHECK_STR(point16(11.24f), "N");   CHECK_STR(point16(11.25f), "NNE");
  CHECK_STR(point16(90), "E");    CHECK_STR(point16(348.75f), "N");  CHECK_STR(point16(348.7f), "NNW");
  CHECK_STR(point16(225), "SW");
  CHECK_EQ(shownHeading(44.6f, -1), 45);
  CHECK_EQ(shownHeading(45.7f, 45), 45);                   // within 0.75: keep
  CHECK_EQ(shownHeading(45.8f, 45), 46);
  CHECK_EQ(shownHeading(359.6f, 0), 0);                    // across north
  CHECK_EQ(shownHeading(359.6f, -1), 0);
  CHECK_EQ(shownHeading(358.9f, 0), 359);
}

TEST(compass_ema_across_north) {
  VectorEma e;
  CHECK(!e.valid());
  e.push(cosf(359 * kD2R), sinf(359 * kD2R), 0.02f, 0.12f);
  for (int i = 0; i < 50; ++i) e.push(cosf(1 * kD2R), sinf(1 * kD2R), 0.02f, 0.12f);
  const float h = headingDeg(e.fwd(), e.left());
  CHECK(fabsf(wrap180(h - 1.0f)) < 0.05f);                 // converges to 1 deg through north, never via 180
  e.reset(); CHECK(!e.valid());
  VectorEma f;
  f.push(1, 0, 0.02f, 0.12f);
  f.push(0, 1, 0.02f, 0.12f);
  const float h1 = headingDeg(f.fwd(), f.left());
  CHECK(h1 > 5.0f && h1 < 15.0f);                          // alpha = 1 - e^(-1/6) ~ 0.154
}

TEST(compass_spring_shortest_path_and_settling) {
  Spring s;
  s.snap(10.0f);
  float minSeen = 400, maxSeen = -1;
  for (int i = 0; i < 100; ++i) {
    const float a = s.update(350.0f, 0.02f, 12.0f);
    const float rel = wrap180(a - 10.0f);
    if (rel < minSeen) minSeen = rel;
    if (rel > maxSeen) maxSeen = rel;
  }
  CHECK(maxSeen <= 0.01f);                                 // went down through north, never up through 180
  CHECK(minSeen >= -20.5f);                                // no big overshoot
  Spring t;
  t.snap(0.0f);
  float tSettle = -1;
  for (int i = 1; i <= 100; ++i) {
    t.update(10.0f, 0.01f, 12.0f);
    if (t.settled() && tSettle < 0) tSettle = i * 0.01f;
  }
  CHECK(tSettle > 0 && tSettle < 0.6f);
  Spring big;                                              // dt clamp: a 1 s frame does not explode
  big.snap(0);
  big.update(170.0f, 1.0f, 24.0f);
  CHECK(std::isfinite(big.angle()));
}
