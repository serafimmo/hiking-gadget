#include "compass_math.h"

#include <math.h>

namespace hg {
namespace {
constexpr float kRadToDeg = 57.29577951308232f;
constexpr float kDegToRad = 0.017453292519943295f;
}  // namespace

Mount mountFromSetting(uint8_t v) { return Mount{(uint8_t)(v & 3u), ((v >> 2) & 1u) != 0}; }

void toDevice(const MagCal& c, const Mount& m, float bx, float by, float& F, float& L) {
  float xc = bx, yc = by;
  if (c.valid) {
    xc = (bx - c.offX) * c.scaleX;
    yc = (by - c.offY) * c.scaleY;
  }
  if (m.chipDown) yc = -yc;
  // F = xc*cos(t) + yc*sin(t); L = -xc*sin(t) + yc*cos(t), t = rot * 90 deg (exact)
  switch (m.rot & 3u) {
    case 0: F = xc;  L = yc;  break;
    case 1: F = yc;  L = -xc; break;
    case 2: F = -xc; L = -yc; break;
    default: F = -yc; L = xc; break;
  }
}

float wrap360(float deg) {
  float d = fmodf(deg, 360.0f);
  if (d < 0) d += 360.0f;
  return d >= 360.0f ? 0.0f : d;
}

float wrap180(float deg) {
  float d = wrap360(deg);
  return d > 180.0f ? d - 360.0f : d;
}

float headingDeg(float F, float L) { return wrap360(atan2f(L, F) * kRadToDeg); }

const char* point16(float deg) {
  static const char* const kNames[16] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
                                         "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  return kNames[(int)floorf((wrap360(deg) + 11.25f) / 22.5f) % 16];
}

int16_t shownHeading(float deg, int16_t prevShown) {
  const float d = wrap360(deg);
  if (prevShown >= 0 && prevShown < 360 && fabsf(wrap180(d - (float)prevShown)) <= 0.75f) return prevShown;
  int16_t r = (int16_t)lroundf(d);
  return r >= 360 ? (int16_t)(r - 360) : r;
}

void VectorEma::reset() { valid_ = false; f_ = l_ = 0; }

void VectorEma::push(float F, float L, float dtS, float tauS) {
  if (!valid_ || tauS <= 0) {
    f_ = F;
    l_ = L;
    valid_ = true;
    return;
  }
  const float a = 1.0f - expf(-(dtS > 0 ? dtS : 0) / tauS);
  f_ += a * (F - f_);
  l_ += a * (L - l_);
}

bool VectorEma::valid() const { return valid_; }
float VectorEma::fwd() const { return f_; }
float VectorEma::left() const { return l_; }

void Spring::snap(float deg) { a_ = wrap360(deg); w_ = 0; e_ = 0; }

float Spring::update(float targetDeg, float dtS, float omega) {
  if (dtS > 0.05f) dtS = 0.05f;           // a slow frame drops animation frames, never destabilises the spring
  if (dtS < 0) dtS = 0;
  e_ = wrap180(targetDeg - a_);           // shortest path, no 359 -> 0 spin
  w_ += (omega * omega * e_ - 2.0f * omega * w_) * dtS;   // semi-implicit Euler (velocity first)
  a_ = wrap360(a_ + w_ * dtS);
  e_ = wrap180(targetDeg - a_);
  return a_;
}

float Spring::angle() const { return a_; }
bool Spring::settled() const { return fabsf(w_) < 1.0f && fabsf(e_) < 0.3f; }

}  // namespace hg
