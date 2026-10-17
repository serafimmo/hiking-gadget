#include "logger.h"

#include <stdio.h>
#include <string.h>

namespace hg {
namespace {
constexpr size_t kMaxPerPump = 1024;
const char kLevel[4] = {'E', 'W', 'I', 'D'};
}  // namespace

bool Logger::push(const char* s, size_t n) {
  if (n > kFifo - used_) {
    droppedPending_ += (uint32_t)n;
    droppedTotal_ += (uint32_t)n;
    return false;
  }
  for (size_t i = 0; i < n; ++i) fifo_[(head_ + used_ + i) % kFifo] = s[i];
  used_ += n;
  return true;
}

void Logger::vlog(LogLevel l, const char* tag, const char* fmt, va_list ap) {
  if ((uint8_t)l > (uint8_t)level_) return;
  char msg[LogRing::kLen];
  const int64_t ms = mono_ ? mono_->now().v / 1000 : 0;
  int n = snprintf(msg, sizeof msg, "[%5d.%03d] %c %-4s ", (int)(ms / 1000 % 100000), (int)(ms % 1000), kLevel[(int)l], tag);
  if (n < 0) n = 0;
  if ((size_t)n < sizeof msg) vsnprintf(msg + n, sizeof msg - (size_t)n, fmt, ap);
  ring_.push(msg);
  char out[LogRing::kLen + 2];
  const int m = snprintf(out, sizeof out, "%s\r\n", msg);
  if (m > 0) push(out, (size_t)m < sizeof out ? (size_t)m : sizeof out - 1);
}

void Logger::log(LogLevel l, const char* tag, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vlog(l, tag, fmt, ap);
  va_end(ap);
}

void Logger::reply(const char* fmt, ...) {
  char out[160];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(out, sizeof out - 2, fmt, ap);
  va_end(ap);
  if (n < 0) return;
  if ((size_t)n > sizeof out - 3) n = (int)sizeof out - 3;
  out[n++] = '\r';
  out[n++] = '\n';
  push(out, (size_t)n);
}

bool Logger::startLong(LineGen gen, void* ctx) {
  if (gen_) return false;
  gen_ = gen;
  genCtx_ = ctx;
  genIndex_ = 0;
  lineLen_ = lineOff_ = 0;
  beforeLong_ = used_;
  return true;
}

bool Logger::writeFifo(size_t maxBytes, size_t& budget) {
  while (maxBytes && used_ && budget) {
    size_t chunk = kFifo - head_;
    if (chunk > used_) chunk = used_;
    if (chunk > maxBytes) chunk = maxBytes;
    if (chunk > budget) chunk = budget;
    const size_t w = serial_->write(fifo_ + head_, chunk);
    head_ = (head_ + w) % kFifo;
    used_ -= w;
    budget -= w;
    maxBytes -= w;
    if (beforeLong_) beforeLong_ = beforeLong_ > w ? beforeLong_ - w : 0;
    if (w < chunk) return false;   // the TX ring is full: continue next loop
  }
  return true;
}

void Logger::pump() {
  if (!serial_) return;
  size_t budget = serial_->writable();
  if (budget > kMaxPerPump) budget = kMaxPerPump;
  if (!budget) return;
  if (droppedPending_) {   // report the loss first, once output resumes
    char d[40];
    const int n = snprintf(d, sizeof d, "[%lu bytes dropped]\r\n", (unsigned long)droppedPending_);
    if ((size_t)n > budget) return;
    const size_t w = serial_->write(d, (size_t)n);
    if (w < (size_t)n) return;   // try again whole next time
    budget -= w;
    droppedPending_ = 0;
  }
  if (!writeFifo(gen_ ? beforeLong_ : used_, budget)) return;
  while (gen_ && budget) {
    if (lineOff_ == lineLen_) {
      if (!gen_(genCtx_, genIndex_++, line_, sizeof line_ - 2)) { gen_ = nullptr; break; }
      lineLen_ = strlen(line_);
      line_[lineLen_++] = '\r';
      line_[lineLen_++] = '\n';
      lineOff_ = 0;
    }
    size_t chunk = lineLen_ - lineOff_;
    if (chunk > budget) chunk = budget;
    const size_t w = serial_->write(line_ + lineOff_, chunk);
    lineOff_ += w;
    budget -= w;
    if (w < chunk) return;
  }
  if (!gen_) writeFifo(used_, budget);
}

}  // namespace hg
