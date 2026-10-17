// core/countdown (SPEC §2.5, §10.3).
#include "core/countdown.h"
#include "tiny_test.h"

using namespace hg;

namespace {
MonoUs at(int64_t ms) { return MonoUs{ms * 1000}; }
}  // namespace

TEST(tm_preset_cycle) {
  Countdown c;
  CHECK_EQ(c.preset(), 0);
  for (int i = 1; i <= 6; ++i) { c.nextPreset(); CHECK_EQ(c.preset(), i % 6); }
  c.setPreset(3);
  CHECK_EQ(c.totalUs(), (int64_t)600 * kUsPerS);
  c.setPreset(9);                                           // invalid: ignored
  CHECK_EQ(c.preset(), 3);
}

TEST(tm_ceil_display) {
  CHECK_EQ(Countdown::displaySeconds(4999999), (int64_t)5);
  CHECK_EQ(Countdown::displaySeconds(5000000), (int64_t)5);
  CHECK_EQ(Countdown::displaySeconds(1), (int64_t)1);
  CHECK_EQ(Countdown::displaySeconds(0), (int64_t)0);
  CHECK_EQ(Countdown::displaySeconds(-5), (int64_t)0);
}

TEST(tm_tick_fires_exactly_once_at_the_deadline) {
  Countdown c;
  c.setPreset(2);                                           // 5 min
  c.start(at(1000));
  CHECK_EQ(Countdown::displaySeconds(c.remainingUs(at(1000))), (int64_t)300);   // shows 05:00 at the start
  CHECK(!c.tick(at(1000 + 299999)));
  CHECK_EQ(Countdown::displaySeconds(c.remainingUs(at(1000 + 299999))), (int64_t)1);
  CHECK(c.tick(at(1000 + 300000)));                         // fires exactly when 00:00 would appear
  CHECK(!c.tick(at(1000 + 300001)));
  CHECK_EQ(c.state(), Countdown::State::Done);
  CHECK_EQ(c.overtimeUs(at(1000 + 300000 + 4500)), (int64_t)4500000);
}

TEST(tm_add_minute_and_cap) {
  Countdown c;
  CHECK(!c.addMinute());                                    // not running
  c.setPreset(5);                                           // 60 min
  c.start(at(0));
  CHECK(c.addMinute());
  CHECK_EQ(c.totalUs(), (int64_t)61 * 60 * kUsPerS);
  CHECK_EQ(c.remainingUs(at(0)), (int64_t)61 * 60 * kUsPerS);
  for (int i = 0; i < 539; ++i) CHECK(c.addMinute());       // 600 min total = 10 h
  CHECK_EQ(c.totalUs(), Countdown::kMaxTotalUs);
  CHECK(!c.addMinute());                                    // "Max 10 h", no change
  CHECK_EQ(c.totalUs(), Countdown::kMaxTotalUs);
}

TEST(tm_pause_math_cancel_restart_toSelect) {
  Countdown c;
  c.start(at(0));                                           // 1 min
  c.pause(at(20000));
  CHECK_EQ(c.state(), Countdown::State::Paused);
  CHECK_EQ(c.remainingUs(at(99999)), (int64_t)40 * kUsPerS);
  c.resume(at(100000));
  CHECK_EQ(c.deadline().v, (int64_t)140000 * 1000);
  CHECK(!c.tick(at(139999)));
  c.pause(at(130000));
  c.cancel();
  CHECK_EQ(c.state(), Countdown::State::Select);
  CHECK_EQ(c.preset(), 0);
  c.start(at(0));
  CHECK(c.tick(at(60000)));
  c.restart(at(70000));
  CHECK_EQ(c.state(), Countdown::State::Running);
  CHECK_EQ(c.remainingUs(at(70000)), (int64_t)60 * kUsPerS);
  CHECK(c.tick(at(130000)));
  c.toSelect();
  CHECK_EQ(c.state(), Countdown::State::Select);
  c.start(at(0));
  c.pause(at(60000));                                       // at the deadline: ignored, tick() ends it
  CHECK_EQ(c.state(), Countdown::State::Running);
  CHECK(c.tick(at(60000)));
}

TEST(tm_snapshot_restore) {
  Countdown c;
  c.setPreset(3);                                           // 10 min
  c.start(at(0));
  const Countdown::Snapshot s = c.save(at(60000));          // 9 min left
  Countdown r;
  r.restore(s, 300LL * kUsPerS, at(10));                    // slept 5 min
  CHECK_EQ(r.state(), Countdown::State::Running);
  CHECK_EQ(r.remainingUs(at(10)), (int64_t)240 * kUsPerS);
  Countdown late;
  late.restore(s, 600LL * kUsPerS, at(10));                 // slept 10 min: expired 1 min ago
  CHECK(late.tick(at(10)));                                 // fires once on the first tick
  CHECK(!late.tick(at(20)));
  CHECK_EQ(late.overtimeUs(at(10)), (int64_t)60 * kUsPerS);
  Countdown d;
  d.start(at(0));
  d.tick(at(60000));
  Countdown dr;
  dr.restore(d.save(at(65000)), 10LL * kUsPerS, at(0));     // Done: overtime += slept
  CHECK_EQ(dr.state(), Countdown::State::Done);
  CHECK_EQ(dr.overtimeUs(at(0)), (int64_t)15 * kUsPerS);
  Countdown::Snapshot bad = s;
  bad.totalUs = -1;
  Countdown b;
  b.restore(bad, 0, at(0));
  CHECK_EQ(b.state(), Countdown::State::Select);
  CHECK_EQ(b.preset(), 3);
}
