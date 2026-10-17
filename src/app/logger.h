#pragma once
// Log API -> LogRing + paced output to ISerialPort (SPEC §7.3, §9.1). Nothing is truncated silently: live lines
// that do not fit the FIFO are counted and reported as "[N bytes dropped]" when output resumes.
#include <stdarg.h>

#include "../core/log_ring.h"
#include "ports.h"

namespace hg {
enum class LogLevel : uint8_t { E, W, I, D };

class Logger {
 public:
  static constexpr size_t kFifo = 1024;
  // Long outputs (boot report, INFO, HELP, LOG DUMP, replay) are generated line by line from their source.
  using LineGen = bool (*)(void* ctx, uint16_t index, char* buf, size_t n);   // false = no more lines
  Logger(ISerialPort* serial, const IMonoClock* mono) : serial_(serial), mono_(mono) {}
  void setLevel(LogLevel l) { level_ = l; }
  LogLevel level() const { return level_; }
  void log(LogLevel l, const char* tag, const char* fmt, ...) __attribute__((format(printf, 4, 5)));
  void vlog(LogLevel l, const char* tag, const char* fmt, va_list ap);
  void reply(const char* fmt, ...) __attribute__((format(printf, 2, 3)));   // console replies: no prefix, not in the ring
  bool startLong(LineGen gen, void* ctx);   // false if another long output is still running
  bool longActive() const { return gen_ != nullptr; }
  void pump();                              // once per loop: at most writable() bytes, <= 1 KB
  uint32_t droppedBytes() const { return droppedTotal_; }
  const LogRing& ring() const { return ring_; }

 private:
  bool push(const char* s, size_t n);       // whole line into the FIFO, or count it as dropped
  size_t fifoUsed() const { return used_; }
  bool writeFifo(size_t maxBytes, size_t& budget);
  ISerialPort* serial_;
  const IMonoClock* mono_;
  LogRing ring_;
  LogLevel level_ = LogLevel::I;
  char fifo_[kFifo] = {};
  size_t head_ = 0, used_ = 0;
  size_t beforeLong_ = 0;                   // FIFO bytes that were queued before the long output started
  uint32_t droppedPending_ = 0, droppedTotal_ = 0;
  LineGen gen_ = nullptr;
  void* genCtx_ = nullptr;
  uint16_t genIndex_ = 0;
  char line_[128] = {};
  size_t lineLen_ = 0, lineOff_ = 0;
};
}  // namespace hg
