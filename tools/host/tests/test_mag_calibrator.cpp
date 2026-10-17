// core/mag_calibrator (SPEC §6.4): synthetic ellipse, partial turn, tilt rejection, weak field.
#include <cmath>
#include <cstdint>

#include "core/mag_calibrator.h"
#include "tiny_test.h"

using namespace hg;

namespace {
constexpr float kD2R = 0.017453292519943295f;
struct Lcg { uint32_t s = 12345; float u() { s = s * 1664525u + 1013904223u; return (float)(s >> 8) / 16777216.0f; }
             float n() { float a = 0; for (int i = 0; i < 6; ++i) a += u(); return (a - 3.0f) * 0.7f; } };

struct Turn { float offX = 500, offY = -500, sx = 0.8f, sy = 1.2f, H = 16.6f * 30, zUp = 49.0f * 30;
              float noise = 4, outlierRate = 0.05f, tiltDeg = 0; bool tiltHalf = false; };

// Feeds `turns` slow turns (360 deg in 12 s at 50 Hz) and returns the calibrator.
MagCalibrator run(const Turn& t, float turns, int64_t& endUs, float lsbPerUt = 30) {
  MagCalibrator c;
  Lcg r;
  c.begin(1, lsbPerUt, MonoUs{0});
  const int n = (int)(turns * 12 * 50);
  int64_t now = 0;
  for (int i = 0; i < n; ++i, now += 20000) {
    const float psi = (i * 360.0f / 600.0f) * kD2R;
    const float tilt = (t.tiltHalf && i % 600 >= 300) ? t.tiltDeg * kD2R : 0;
    // horizontal field H at angle psi in the sensor plane; tilt mixes the vertical component into x and z
    float x = t.H * cosf(psi), y = t.H * sinf(psi), z = t.zUp;
    const float x2 = x * cosf(tilt) + z * sinf(tilt), z2 = -x * sinf(tilt) + z * cosf(tilt);
    x = x2; z = z2;
    float bx = x / t.sx + t.offX + r.n() * t.noise, by = y / t.sy + t.offY + r.n() * t.noise, bz = z + r.n() * t.noise;
    if (r.u() < t.outlierRate) { bx += (r.u() - 0.5f) * 4000; by += (r.u() - 0.5f) * 4000; }   // isolated spikes
    c.add((int16_t)lroundf(bx), (int16_t)lroundf(by), (int16_t)lroundf(bz), false, MonoUs{now});
  }
  endUs = now;
  return c;
}
}  // namespace

TEST(cal_synthetic_ellipse_recovered) {
  // SPEC §10.3 case: offsets +-500, scales 0.8/1.2, noise, 5 % outliers. Its axis ratio (0.67) is below the
  // OK limit of the §6.4 grade table (0.70), so it must be recovered precisely AND graded Poor (strong soft iron).
  Turn t;
  int64_t end = 0;
  MagCalibrator c = run(t, 2.0f, end);
  const MagCalibrator::Result r = c.result();
  CHECK_NEAR(r.cal.offX, 500.0f, 0.02f * 500);
  CHECK_NEAR(r.cal.offY, -500.0f, 0.02f * 500);
  // scaleX/scaleY must undo sx/sy up to a common factor: scaleX/scaleY ~ sx/sy
  CHECK_NEAR((r.cal.scaleX / r.cal.scaleY) / (t.sx / t.sy), 1.0f, 0.02f);
  CHECK(r.coveragePct >= 95);
  CHECK_NEAR(r.axisRatio, 0.8f / 1.2f, 0.02f);
  CHECK_EQ(r.grade, MagCalibrator::Grade::Poor);
  CHECK(!r.cal.valid);
  // residual heading error over a clean turn < 1 deg
  float worst = 0;
  for (int h = 0; h < 360; h += 5) {
    const float psi = h * kD2R;
    const float bx = t.H * cosf(psi) / t.sx + t.offX, by = t.H * sinf(psi) / t.sy + t.offY;
    const float xc = (bx - r.cal.offX) * r.cal.scaleX, yc = (by - r.cal.offY) * r.cal.scaleY;
    const float e = fabsf(wrap180(wrap360(atan2f(yc, xc) / kD2R) - (float)h));
    if (e > worst) worst = e;
  }
  CHECK(worst < 1.0f);
}

TEST(cal_mild_ellipse_good_and_ready) {
  Turn t;
  t.sx = 0.95f; t.sy = 1.05f;
  int64_t end = 0;
  MagCalibrator c = run(t, 2.0f, end);
  const MagCalibrator::Result r = c.result();
  CHECK(r.cal.valid);
  CHECK_EQ(r.grade, MagCalibrator::Grade::Good);
  CHECK(r.cal.qualityPct >= 92);
  CHECK_NEAR(r.fieldUt, 16.6f, 16.6f * 0.05f);
  const MagCalibrator::Progress p = c.progress(MonoUs{end});
  CHECK_EQ(p.verdict, MagCalibrator::Verdict::Ready);
  CHECK_EQ(p.hint, MagCalibrator::Hint::Ready);
  CHECK_EQ(c.progress(MonoUs{7 * kUsPerS}).verdict == MagCalibrator::Verdict::Ready, false);   // < 8 s: never ready
}

TEST(cal_partial_turn_not_ready) {
  Turn t;
  int64_t end = 0;
  MagCalibrator c = run(t, 0.6f, end);
  const MagCalibrator::Progress p = c.progress(MonoUs{end});
  CHECK_EQ(p.verdict, MagCalibrator::Verdict::Collecting);
  CHECK_EQ(p.hint, MagCalibrator::Hint::TurnMore);
  CHECK(p.coveragePct < 90);
  CHECK_EQ(c.result().grade, MagCalibrator::Grade::Poor);
}

TEST(cal_tilt_rejected_with_keep_level) {
  Turn t;
  t.tiltDeg = 15; t.tiltHalf = true;
  int64_t end = 0;
  MagCalibrator c = run(t, 1.0f, end);                     // the second half of the turn is tilted
  const MagCalibrator::Progress p = c.progress(MonoUs{end});
  CHECK_EQ(p.hint, MagCalibrator::Hint::KeepLevel);
  // the tilted samples did not distort the fit: offsets still close
  const MagCalibrator::Result r = c.result();
  CHECK_NEAR(r.cal.offX, 500.0f, 0.05f * 500);
}

TEST(cal_weak_field_poor_and_timeout) {
  Turn t;
  t.H = 3.0f * 30;                                         // 3 uT horizontal
  t.noise = 1; t.outlierRate = 0;
  int64_t end = 0;
  MagCalibrator c = run(t, 2.0f, end);
  const MagCalibrator::Result r = c.result();
  CHECK_EQ(r.grade, MagCalibrator::Grade::Poor);
  CHECK_EQ(r.reason, MagCalibrator::Hint::FieldWeak);
  CHECK(!r.cal.valid);
  CHECK_EQ(c.progress(MonoUs{121LL * kUsPerS}).verdict, MagCalibrator::Verdict::Failed);
}
