#pragma once
#include "types.h"
namespace hg {
class LogRing { public: static constexpr uint8_t kLines = 48, kLen = 96;
  void push(const char* line);  uint8_t count() const;  const char* line(uint8_t oldestFirst) const;
 private:
  char lines_[kLines][kLen] = {};
  uint8_t head_ = 0, count_ = 0; };
}  // namespace hg
