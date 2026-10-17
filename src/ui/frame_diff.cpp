#include "frame_diff.h"

#include <string.h>

namespace hg {

void FrameDiff::invalidate() { force_ = 0xFF; }

bool FrameDiff::clean(const uint8_t* frame) const { return force_ == 0 && memcmp(frame, shadow_, sizeof shadow_) == 0; }

uint8_t FrameDiff::collect(const uint8_t* frame, PageSpan* out, uint8_t max) {
  uint8_t n = 0;
  const uint8_t start = cursor_;
  for (uint8_t k = 0; k < 8 && n < max; ++k) {
    const uint8_t page = (uint8_t)((start + k) % 8);
    const uint8_t* f = frame + page * 128;
    uint8_t* s = shadow_ + page * 128;
    int first = -1, last = -1;
    if (force_ & (1u << page)) {
      first = 0;
      last = 15;
    } else {
      for (int t = 0; t < 16; ++t)
        if (memcmp(f + t * 8, s + t * 8, 8) != 0) { if (first < 0) first = t; last = t; }
    }
    if (first < 0) continue;
    out[n++] = PageSpan{page, (uint8_t)first, (uint8_t)(last - first + 1)};
    memcpy(s + first * 8, f + first * 8, (size_t)(last - first + 1) * 8);
    force_ &= (uint8_t)~(1u << page);
    cursor_ = (uint8_t)((page + 1) % 8);
  }
  return n;
}

}  // namespace hg
