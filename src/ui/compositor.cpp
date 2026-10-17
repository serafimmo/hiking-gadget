#include "compositor.h"

namespace hg {

uint64_t frameColumn(const uint8_t* buf, uint8_t x) {
  uint64_t v = 0;
  for (int p = 0; p < 8; ++p) v |= (uint64_t)buf[p * 128 + x] << (8 * p);
  return v;
}

void setFrameColumn(uint8_t* buf, uint8_t x, uint64_t v) {
  for (int p = 0; p < 8; ++p) buf[p * 128 + x] = (uint8_t)(v >> (8 * p));
}

uint64_t slideColumn(uint64_t oldCol, uint64_t newCol, uint8_t d, int8_t dir) {
  if (d == 0) return oldCol;            // shifting a 64-bit value by 64 is undefined: both ends are special-cased
  if (d >= 64) return newCol;
  return dir > 0 ? (oldCol >> d) | (newCol << (64 - d)) : (oldCol << d) | (newCol >> (64 - d));
}

void composeSlide(const uint8_t* oldFrame, uint8_t* live, uint8_t d, int8_t dir, uint8_t x1) {
  for (uint16_t x = 0; x <= x1 && x < 128; ++x)
    setFrameColumn(live, (uint8_t)x, slideColumn(frameColumn(oldFrame, (uint8_t)x), frameColumn(live, (uint8_t)x), d, dir));
}

uint8_t slideOffset(int64_t elapsedUs, int64_t durationUs) {
  if (durationUs <= 0 || elapsedUs >= durationUs) return 64;
  if (elapsedUs <= 0) return 0;
  const double t = (double)elapsedUs / (double)durationUs, u = 1.0 - t;
  const int d = (int)(64.0 * (1.0 - u * u * u) + 0.5);
  return (uint8_t)(d > 64 ? 64 : d);
}

}  // namespace hg
