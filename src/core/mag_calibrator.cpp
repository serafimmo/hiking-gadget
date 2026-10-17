#include "mag_calibrator.h"

#include <math.h>

#include <new>
#include <type_traits>

#include "../config/timing.h"

namespace hg {
namespace {
constexpr float kRadToDeg = 57.29577951308232f;
int16_t median3(int16_t a, int16_t b, int16_t c) {
  if (a > b) { int16_t t = a; a = b; b = t; }
  if (b > c) { int16_t t = b; b = c; c = t; }
  return a > b ? a : b;
}
int countBits(uint64_t v) { int n = 0; for (; v; v &= v - 1) ++n; return n; }
}  // namespace

void MagCalibrator::begin(uint8_t chip, float lsbPerUt, MonoUs now) {
  static_assert(std::is_trivially_destructible<MagCalibrator>::value, "re-constructed in place");
  new (this) MagCalibrator();   // in place: '*this = MagCalibrator{}' put a 3 KB temporary on the 8 KB loop stack
  chip_ = chip;
  lsbPerUt_ = lsbPerUt > 0 ? lsbPerUt : 1;
  t0_ = now;
  started_ = true;
}

bool MagCalibrator::haveSpread() const {
  // The running estimate is meaningful once both axes span a few uT.
  return haveMinMax_ && (maxX_ - minX_) > 4 * lsbPerUt_ && (maxY_ - minY_) > 4 * lsbPerUt_;
}

float MagCalibrator::angleOf(float x, float y) const {
  const float ox = (maxX_ + minX_) * 0.5f, oy = (maxY_ + minY_) * 0.5f;
  return wrap360(atan2f(y - oy, x - ox) * kRadToDeg);
}

void MagCalibrator::add(int16_t bx, int16_t by, int16_t bz, bool overflow, MonoUs now) {
  if (!started_) return;
  lastAdd_ = now;
  if (overflow) {                                   // never used for min/max (hint "Metal near?")
    anyOverflow_ = true;
    lastOverflow_ = now;
    return;
  }
  for (int a = 0; a < 3; ++a) { med_[a][0] = med_[a][1]; med_[a][1] = med_[a][2]; }
  med_[0][2] = bx; med_[1][2] = by; med_[2][2] = bz;
  if (medN_ < 3) { ++medN_; if (medN_ < 3) return; }
  const int16_t x = median3(med_[0][0], med_[0][1], med_[0][2]);
  const int16_t y = median3(med_[1][0], med_[1][1], med_[1][2]);
  const int16_t z = median3(med_[2][0], med_[2][1], med_[2][2]);

  // zRef = median z of the first 2 s
  if (!zRefSet_) {
    if (now - t0_ < 2 * kUsPerS) {
      if (zRefN_ < kZRefMax) zRefBuf_[zRefN_++] = z;
    } else {
      for (int i = 1; i < zRefN_; ++i)            // insertion sort, <= 128 values once
        for (int j = i; j > 0 && zRefBuf_[j - 1] > zRefBuf_[j]; --j) { int16_t t = zRefBuf_[j]; zRefBuf_[j] = zRefBuf_[j - 1]; zRefBuf_[j - 1] = t; }
      zRef_ = zRefN_ ? zRefBuf_[zRefN_ / 2] : z;
      zRefSet_ = true;
    }
  }
  // Level check: |z - zRef| > 0.15 * r  (~9 deg of tilt) -> rejected
  bool rejected = false;
  if (zRefSet_ && haveSpread()) {
    const float r = ((maxX_ - minX_) + (maxY_ - minY_)) * 0.25f;
    rejected = fabsf(z - zRef_) > 0.15f * r;
  }
  rejectHist_ = (rejectHist_ << 1) | (rejected ? 1u : 0u);
  if (histN_ < 50) ++histN_;
  if (rejected) return;

  // Jump gate: a slow turn moves the vector by a few % of r per 20 ms sample; a spike moves it by far more.
  // Two spikes in a row get through the median-of-3, never through this. After 5 rejections in a row the
  // field really moved (pause in the data): accept and re-anchor.
  if (haveRef_) {
    const float gate = haveSpread() ? 0.15f * ((maxX_ - minX_) + (maxY_ - minY_)) * 0.25f : 15.0f * lsbPerUt_;
    const float g = gate > 3.0f * lsbPerUt_ ? gate : 3.0f * lsbPerUt_;
    if (fabsf((float)(x - refX_)) > g || fabsf((float)(y - refY_)) > g) {
      if (++jumpRejects_ < 5) return;
    }
  }
  jumpRejects_ = 0;
  refX_ = x; refY_ = y; haveRef_ = true;

  if (!haveMinMax_) { minX_ = maxX_ = x; minY_ = maxY_ = y; haveMinMax_ = true; }
  if (x < minX_) minX_ = x;
  if (x > maxX_) maxX_ = x;
  if (y < minY_) minY_ = y;
  if (y > maxY_) maxY_ = y;
  lastX_ = x; lastY_ = y;
  if (!haveSpread()) return;

  const float ang = angleOf(x, y);
  Slot& s = slots_[(int)ang % kSlots];
  s = Slot{x, y, z, true};
  // Angular rate over >= 200 ms
  if (!rateInit_) { rateInit_ = true; rateAngle_ = ang; rateT_ = now; }
  else if (now - rateT_ >= 200 * kUsPerMs) {
    rateDps_ = fabsf(wrap180(ang - rateAngle_)) / ((float)(now - rateT_) / kUsPerS);
    rateAngle_ = ang;
    rateT_ = now;
  }
}

MagCalibrator::Fit MagCalibrator::fit() const {
  Fit f{};
  if (!haveSpread()) return f;
  f.offX = (maxX_ + minX_) * 0.5f;
  f.offY = (maxY_ + minY_) * 0.5f;
  f.rX = (maxX_ - minX_) * 0.5f;
  f.rY = (maxY_ - minY_) * 0.5f;
  f.rMean = (f.rX + f.rY) * 0.5f;
  f.ratio = f.rY / f.rX;
  f.fieldUt = f.rMean / lsbPerUt_;
  const float sx = f.rMean / f.rX, sy = f.rMean / f.rY;
  uint8_t binCount[24] = {};
  double sum = 0, sum2 = 0;
  int n = 0;
  for (const Slot& s : slots_) {
    if (!s.used) continue;
    const float xc = (s.x - f.offX) * sx, yc = (s.y - f.offY) * sy;
    const float r = sqrtf(xc * xc + yc * yc);
    sum += r; sum2 += (double)r * r; ++n;
    if (r >= 0.5f * f.rMean) {
      const int b = (int)(wrap360(atan2f(s.y - f.offY, s.x - f.offX) * kRadToDeg) / 15.0f) % 24;
      if (binCount[b] < 255) ++binCount[b];
    }
  }
  for (int b = 0; b < 24; ++b)
    if (binCount[b] >= 2) { f.mask |= 1u << b; ++f.bins; }
  if (n > 1) {
    const double mean = sum / n;
    const double var = sum2 / n - mean * mean;
    f.q = mean > 0 ? (float)(1.0 - sqrt(var > 0 ? var : 0) / mean) : 0;
  }
  return f;
}

MagCalibrator::Progress MagCalibrator::progress(MonoUs now) const {
  Progress p{};
  const Fit f = fit();
  const Result r = result();
  p.binsMask = f.mask;
  p.coveragePct = (uint8_t)(f.bins * 100 / 24);
  p.fieldUt = f.fieldUt;
  p.liveDeg = haveSpread() ? angleOf(lastX_, lastY_) : 0;
  const int64_t elapsed = now - t0_;
  const bool ready = f.bins >= 22 && r.grade != Grade::Poor && elapsed >= (int64_t)kCalMinMs * kUsPerMs;
  p.verdict = ready ? Verdict::Ready : elapsed >= (int64_t)kCalTimeoutMs * kUsPerMs ? Verdict::Failed : Verdict::Collecting;
  if (ready) p.hint = Hint::Ready;
  else if (anyOverflow_ && now - lastOverflow_ < 2 * kUsPerS) p.hint = Hint::MetalNear;
  else if (histN_ >= 50 && countBits(rejectHist_ & ((1ULL << 50) - 1)) > 10) p.hint = Hint::KeepLevel;
  else if (rateDps_ > 90.0f && now - rateT_ < 1 * kUsPerS) p.hint = Hint::TurnSlower;
  else if (haveSpread() && f.bins >= 12 && f.fieldUt > 60.0f) p.hint = Hint::MetalNear;
  else if (haveSpread() && f.bins >= 12 && f.fieldUt < 5.0f) p.hint = Hint::FieldWeak;
  else p.hint = Hint::TurnMore;
  return p;
}

MagCalibrator::Result MagCalibrator::result() const {
  Result r{};
  const Fit f = fit();
  r.fieldUt = f.fieldUt;
  r.axisRatio = f.ratio;
  r.coveragePct = (uint8_t)(f.bins * 100 / 24);
  r.cal.chip = chip_;
  r.cal.lsbPerUt = lsbPerUt_;
  if (!haveSpread()) { r.grade = Grade::Poor; r.reason = Hint::TurnMore; return r; }
  r.cal.offX = f.offX; r.cal.offY = f.offY;
  r.cal.scaleX = f.rMean / f.rX; r.cal.scaleY = f.rMean / f.rY;
  r.cal.rCalCounts = f.rMean;
  r.cal.qualityPct = (uint8_t)(f.q > 0 ? (f.q >= 1 ? 100 : f.q * 100.0f + 0.5f) : 0);
  if (f.bins >= 23 && f.ratio >= 0.85f && f.ratio <= 1.15f && f.q >= 0.92f) r.grade = Grade::Good;
  else if (f.bins >= 22 && f.ratio >= 0.70f && f.ratio <= 1.40f && f.q >= 0.85f) r.grade = Grade::Ok;
  else r.grade = Grade::Poor;
  if (f.fieldUt < 5.0f || f.fieldUt > 60.0f) r.grade = Grade::Poor;
  if (r.grade == Grade::Poor) {
    r.reason = f.bins < 22 ? Hint::TurnMore : f.fieldUt < 5.0f ? Hint::FieldWeak
             : f.fieldUt > 60.0f ? Hint::MetalNear : histN_ >= 50 && countBits(rejectHist_ & ((1ULL << 50) - 1)) > 10
             ? Hint::KeepLevel : Hint::MetalNear;
  } else {
    r.reason = Hint::Ready;
  }
  r.cal.valid = r.grade != Grade::Poor;
  return r;
}

}  // namespace hg
