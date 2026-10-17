#include "log_ring.h"

namespace hg {

void LogRing::push(const char* line) {
  char* dst = lines_[(head_ + count_) % kLines];
  if (count_ == kLines) head_ = (uint8_t)((head_ + 1) % kLines);   // overwrite the oldest
  else ++count_;
  uint8_t n = 0;
  if (line)
    for (; line[n] && n < kLen - 1; ++n) dst[n] = line[n];
  dst[n] = 0;
}

uint8_t LogRing::count() const { return count_; }

const char* LogRing::line(uint8_t oldestFirst) const {
  return oldestFirst < count_ ? lines_[(head_ + oldestFirst) % kLines] : "";
}

}  // namespace hg
