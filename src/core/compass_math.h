#pragma once
#include "types.h"
namespace hg {
struct MagCal { uint8_t chip; float offX, offY, scaleX, scaleY, rCalCounts, lsbPerUt; uint8_t qualityPct; bool valid; };
struct Mount { uint8_t rot; bool chipDown; };     // rot: silk X arrow at 0/90/180/270 deg clockwise from device forward
Mount mountFromSetting(uint8_t v);                // v 0..7: rot = v & 3, chipDown = v >> 2
void  toDevice(const MagCal& c, const Mount& m, float bx, float by, float& F, float& L);  // board frame -> forward/left
float headingDeg(float F, float L);               // atan2(L, F) -> [0, 360)
float wrap360(float deg);  float wrap180(float deg);
const char* point16(float deg);                   // "N", "NNE", ...
int16_t shownHeading(float deg, int16_t prevShown);   // round with +-0.75 deg hysteresis
class VectorEma { public: void reset(); void push(float F, float L, float dtS, float tauS);
                  bool valid() const; float fwd() const; float left() const;   // F, L (renamed: Arduino defines a macro F())
                  private: bool valid_ = false; float f_ = 0, l_ = 0; };
class Spring    { public: void snap(float deg); float update(float targetDeg, float dtS, float omega);
                  float angle() const; bool settled() const;    // settled: |w| < 1 deg/s and |e| < 0.3 deg
                  private: float a_ = 0, w_ = 0, e_ = 0; };
constexpr float kNeedleTauS[3] = {0.25f, 0.12f, 0.06f};   // VectorEma tau by NeedleSpeed: Calm, Normal, Fast (§6.4)
constexpr float kNeedleOmega[3] = {6.0f, 12.0f, 24.0f};   // Spring omega by NeedleSpeed (§3.6)
}  // namespace hg
