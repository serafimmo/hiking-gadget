#pragma once
// Shadow of what the panel shows -> dirty page spans in 8-px tiles (SPEC §4.5 flush budget).
#include "../core/types.h"

namespace hg {
struct PageSpan { uint8_t page, tx0, tw; };
class FrameDiff {
 public:
  void invalidate();                                       // every page is resent in full (after power-on, every 60 s)
  // Finds up to max dirty spans, starting after the last page sent (no page starves), and copies them into the
  // shadow: the caller sends exactly these spans. Returns the number found (0 = the panel matches the frame).
  uint8_t collect(const uint8_t* frame, PageSpan* out, uint8_t max);
  bool clean(const uint8_t* frame) const;
 private:
  uint8_t shadow_[1024] = {};
  uint8_t force_ = 0xFF;                                   // pages to resend whole (bit = page); all at start
  uint8_t cursor_ = 0;
};
}  // namespace hg
