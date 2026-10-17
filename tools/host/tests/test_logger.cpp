// app/logger with a 256-byte TX ring and short writes (SPEC §7.3, D31).
#include <cstdio>
#include <string>

#include "app/logger.h"
#include "fakes/fake_ports.h"
#include "tiny_test.h"

using namespace hg;

namespace {
struct Mono : IMonoClock { int64_t t = 12345000; MonoUs now() const override { return MonoUs{t}; } };
bool lines(void*, uint16_t i, char* b, size_t n) {
  if (i >= 60) return false;
  snprintf(b, n, "line %02u of a long output that is about sixty bytes long....", i);
  return true;
}
}  // namespace

TEST(logger_long_output_arrives_complete_and_in_order) {
  FakeSerial s;
  Mono m;
  Logger log(&s, &m);
  log.log(LogLevel::I, "BOOT", "first");
  CHECK(log.startLong(lines, nullptr));
  CHECK(!log.startLong(lines, nullptr));                             // one long output at a time
  log.log(LogLevel::I, "BOOT", "after");
  for (int i = 0; i < 200 && (log.longActive() || s.tx.size() || s.out.find("after") == std::string::npos); ++i) {
    log.pump();
    s.drain();
  }
  const std::string& o = s.out;
  CHECK(o.find("first") < o.find("line 00"));
  CHECK(o.find("line 59") < o.find("after"));
  int count = 0;
  for (size_t p = o.find("line "); p != std::string::npos; p = o.find("line ", p + 1)) ++count;
  CHECK_EQ(count, 60);
  CHECK(o.find("line 31 of a long output that is about sixty bytes long....\r\n") != std::string::npos);
  CHECK_EQ(log.droppedBytes(), 0u);
}

TEST(logger_overflow_reports_dropped_bytes_once) {
  FakeSerial s;
  s.connected = false;                                               // nothing drains: the TX ring fills, then the FIFO
  Mono m;
  Logger log(&s, &m);
  for (int i = 0; i < 40; ++i) { log.log(LogLevel::I, "TEST", "message number %d with some padding text", i); log.pump(); }
  CHECK(log.droppedBytes() > 0);
  s.connected = true;
  for (int i = 0; i < 50; ++i) { log.pump(); s.drain(); }
  const std::string& o = s.out;
  CHECK(o.find(" bytes dropped]") != std::string::npos);
  CHECK_EQ(o.find(" bytes dropped]"), o.rfind(" bytes dropped]"));   // printed once
  CHECK(o.find("message number 0 ") == std::string::npos || o.find("message number 0 ") < o.find("dropped"));
}

TEST(logger_level_ring_and_prefix) {
  FakeSerial s;
  Mono m;
  Logger log(&s, &m);
  log.log(LogLevel::D, "KEY", "hidden at level I");
  log.setLevel(LogLevel::D);
  log.log(LogLevel::D, "KEY", "# down");
  log.log(LogLevel::E, "SHT", "E-SHT-NONE nothing");
  CHECK_EQ(log.ring().count(), 2);
  CHECK_STR(log.ring().line(0), "[   12.345] D KEY  # down");
  log.reply("PONG HG2 2.0.0 1");
  log.pump();
  s.drain();
  CHECK(s.out.find("PONG HG2") != std::string::npos);
  CHECK_EQ(log.ring().count(), 2);                                   // replies are not log lines
}
