// core/stopwatch (SPEC §2.5, §10.3).
#include "core/stopwatch.h"
#include "tiny_test.h"

using namespace hg;

namespace {
MonoUs at(int64_t ms) { return MonoUs{ms * 1000}; }
}  // namespace

TEST(sw_start_pause_resume_reset) {
  Stopwatch sw;
  CHECK_EQ(sw.state(), Stopwatch::State::Idle);
  CHECK_EQ(sw.elapsedUs(at(5000)), (int64_t)0);
  sw.start(at(1000));
  CHECK_EQ(sw.elapsedUs(at(3500)), (int64_t)2500000);
  sw.pause(at(4000));
  CHECK_EQ(sw.state(), Stopwatch::State::Paused);
  CHECK_EQ(sw.elapsedUs(at(9000)), (int64_t)3000000);
  sw.resume(at(10000));
  CHECK_EQ(sw.elapsedUs(at(11000)), (int64_t)4000000);
  sw.start(at(12000));                                      // ignored while running
  CHECK_EQ(sw.elapsedUs(at(12000)), (int64_t)5000000);
  sw.reset();
  CHECK_EQ(sw.state(), Stopwatch::State::Idle);
  CHECK_EQ(sw.lapNo(), 0);
}

TEST(sw_laps_and_deltas) {
  Stopwatch sw;
  CHECK(!sw.lap(at(0)));                                    // not running
  sw.start(at(0));
  CHECK(sw.lap(at(10000)));                                 // lap 1 = 10 s
  CHECK(sw.lap(at(22000)));                                 // lap 2 = 12 s
  CHECK(sw.lap(at(31000)));                                 // lap 3 = 9 s
  CHECK_EQ(sw.lapNo(), 3);
  CHECK_EQ(sw.storedLaps(), 3);
  CHECK_EQ(sw.lapUs(0), (int64_t)9000000);
  CHECK_EQ(sw.lapUs(2), (int64_t)10000000);
  CHECK_EQ(sw.lapUs(3), (int64_t)0);
  CHECK_EQ(sw.lapDeltaUs(0), (int64_t)-3000000);            // faster than the previous lap
  CHECK_EQ(sw.lapDeltaUs(1), (int64_t)2000000);
  CHECK_EQ(sw.lapDeltaUs(2), (int64_t)0);                   // no older lap
  CHECK_EQ(sw.currentLapUs(at(35000)), (int64_t)4000000);
  sw.pause(at(36000));
  CHECK(!sw.lap(at(37000)));                                // only while running
}

TEST(sw_99_lap_ring_numbering_past_99_and_999_cap) {
  Stopwatch sw;
  sw.start(at(0));
  for (int i = 1; i <= 150; ++i) CHECK(sw.lap(at(i * 1000 + i)));   // lap i lasts 1000 + 1 ms (lap 1: 1001 ms)
  CHECK_EQ(sw.lapNo(), 150);
  CHECK_EQ(sw.storedLaps(), 99);
  CHECK_EQ(sw.lapUs(0), (int64_t)1001000);
  CHECK_EQ(sw.lapUs(98), (int64_t)1001000);
  CHECK_EQ(sw.currentLapUs(at(150 * 1001 + 500)), (int64_t)500000);
  for (int i = 151; i <= 999; ++i) sw.lap(at(i * 1001));
  CHECK_EQ(sw.lapNo(), 999);
  CHECK(!sw.lap(at(1000 * 1001)));                          // "Max 999 laps"
  CHECK_EQ(sw.lapNo(), 999);
}

TEST(sw_cap_auto_pauses_once) {
  Stopwatch sw;
  sw.start(at(0));
  const MonoUs nearCap{Stopwatch::kMaxElapsedUs - 1000};
  CHECK(!sw.tick(nearCap));
  CHECK(sw.tick(MonoUs{Stopwatch::kMaxElapsedUs + 5000}));
  CHECK_EQ(sw.state(), Stopwatch::State::Paused);
  CHECK_EQ(sw.elapsedUs(MonoUs{Stopwatch::kMaxElapsedUs * 2}), Stopwatch::kMaxElapsedUs);
  CHECK(!sw.tick(MonoUs{Stopwatch::kMaxElapsedUs * 2}));
  sw.resume(at(1));                                         // cannot resume at the cap
  CHECK_EQ(sw.state(), Stopwatch::State::Paused);
}

TEST(sw_snapshot_round_trip) {
  Stopwatch sw;
  sw.start(at(0));
  for (int i = 1; i <= 120; ++i) sw.lap(at(i * 2000));
  const Stopwatch::Snapshot s = sw.save(at(241000));
  Stopwatch r;
  r.restore(s, 600LL * kUsPerS, at(50));                    // 10 min of deep sleep, new boot clock
  CHECK_EQ(r.state(), Stopwatch::State::Running);
  CHECK_EQ(r.elapsedUs(at(50)), (int64_t)(241000 + 600000) * 1000);
  CHECK_EQ(r.lapNo(), 120);
  CHECK_EQ(r.lapUs(0), (int64_t)2000000);
  CHECK_EQ(r.lapUs(98), (int64_t)2000000);
  CHECK_EQ(r.currentLapUs(at(50)), (int64_t)(1000 + 600000) * 1000);
  sw.pause(at(242000));
  Stopwatch p;
  p.restore(sw.save(at(242000)), 99 * kUsPerS, at(0));      // paused: slept time is not added
  CHECK_EQ(p.state(), Stopwatch::State::Paused);
  CHECK_EQ(p.elapsedUs(at(0)), (int64_t)242000 * 1000);
  Stopwatch::Snapshot bad = s;
  bad.state = 7;
  Stopwatch b;
  b.restore(bad, 0, at(0));
  CHECK_EQ(b.state(), Stopwatch::State::Idle);
}
