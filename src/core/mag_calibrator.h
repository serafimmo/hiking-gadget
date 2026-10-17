#pragma once
#include "compass_math.h"
namespace hg {
class MagCalibrator {
 public:
  enum class Verdict : uint8_t { Collecting, Ready, Failed };
  enum class Hint : uint8_t { TurnMore, KeepLevel, TurnSlower, MetalNear, FieldWeak, Ready };
  enum class Grade : uint8_t { Good, Ok, Poor };
  struct Progress { uint32_t binsMask; uint8_t coveragePct; float fieldUt; float liveDeg; Hint hint; Verdict verdict; };
  struct Result { MagCal cal; Grade grade; float fieldUt; float axisRatio; uint8_t coveragePct; Hint reason; };
  void begin(uint8_t chip, float lsbPerUt, MonoUs now);
  void add(int16_t bx, int16_t by, int16_t bz, bool overflow, MonoUs now);   // board frame, <= 50 Hz
  Progress progress(MonoUs now) const;
  Result   result() const;

 private:
  static constexpr int kSlots = 360;       // one slot per degree of the running angle estimate (the "ring of 360")
  static constexpr int kZRefMax = 128;     // z medians of the first 2 s
  struct Slot { int16_t x, y, z; bool used; };
  struct Fit { float offX, offY, rX, rY, rMean, q, ratio, fieldUt; uint8_t bins; uint32_t mask; };
  Fit fit() const;
  bool haveSpread() const;
  float angleOf(float x, float y) const;
  uint8_t chip_ = 0;
  float lsbPerUt_ = 1;
  MonoUs t0_{0}, lastAdd_{0}, lastOverflow_{0}, rateT_{0};
  bool started_ = false, anyOverflow_ = false;
  int16_t med_[3][3] = {};                 // last 3 raw samples per axis (median-of-3)
  uint8_t medN_ = 0;
  int16_t zRefBuf_[kZRefMax] = {};
  uint8_t zRefN_ = 0;
  bool zRefSet_ = false;
  float zRef_ = 0;
  int16_t minX_ = 0, maxX_ = 0, minY_ = 0, maxY_ = 0;
  bool haveMinMax_ = false;
  uint64_t rejectHist_ = 0;                // bit i = sample i (newest = bit 0) was rejected for tilt; 50 kept
  uint8_t histN_ = 0;
  float rateAngle_ = 0, rateDps_ = 0;
  bool rateInit_ = false;
  float lastX_ = 0, lastY_ = 0;
  int16_t refX_ = 0, refY_ = 0;            // last sample that passed the jump gate
  bool haveRef_ = false;
  uint8_t jumpRejects_ = 0;
  Slot slots_[kSlots] = {};
};
}  // namespace hg
