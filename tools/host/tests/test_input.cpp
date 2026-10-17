// core/input: debounce, click/hold/hesitation, repeat/accel, swallow, ownership, latching (D25), stuck keys (D26).
#include "core/input.h"
#include "tiny_test.h"

using namespace hg;

namespace {
using T = InputEvent::Type;
constexpr uint8_t kUp = 1, kDn = 2, kHash = 4, kStar = 8;
const KeyConfig kOnDown{Trigger::OnDown, 0, false, false};
const KeyConfig kClick800{Trigger::OnClick, 800, false, false};

// Drives InputLogic with 5 ms samples and collects events.
struct Rig {
  InputLogic in;
  int64_t tMs = 0;
  InputEvent ev[64];
  int n = 0;
  void feed(uint8_t mask) { in.feed(mask, MonoUs{tMs * 1000}); InputEvent e; while (n < 64 && in.pop(e)) ev[n++] = e; }
  void hold(uint8_t mask, int64_t ms) { for (int64_t end = tMs + ms; tMs < end; tMs += 5) feed(mask); }
  int count(T t) const { int c = 0; for (int i = 0; i < n; ++i) c += ev[i].type == t; return c; }
  int find(T t) const { for (int i = 0; i < n; ++i) if (ev[i].type == t) return i; return -1; }
  void clear() { n = 0; }
};
}  // namespace

TEST(input_chatter_is_one_press_and_tDown_is_first_low_of_the_run) {
  Rig r;
  r.in.configure(Key::Star, kOnDown);
  r.hold(0, 20);
  for (int i = 0; i < 4; ++i) { r.hold(kStar, 5); r.hold(0, 5); }   // 1..20 ms chatter, never 15 ms stable
  CHECK_EQ(r.count(T::Down), 0);
  const int64_t lowStart = r.tMs;
  r.hold(kStar, 100);
  r.hold(0, 50);
  CHECK_EQ(r.count(T::Down), 1);
  CHECK_EQ(r.count(T::Up), 1);
  CHECK_EQ(r.ev[r.find(T::Down)].tDown.v, lowStart * 1000);
}

TEST(input_two_sample_rule) {
  InputLogic in;
  in.configure(Key::Up, kOnDown);
  in.feed(kUp, MonoUs{0});
  in.feed(kUp, MonoUs{100000});      // 100 ms later but only the 2nd sample: accepted now (>= 15 ms and >= 2 samples)
  InputEvent e;
  CHECK(in.pop(e));
  CHECK_EQ(e.type, T::Down);
  InputLogic one;
  one.feed(kUp, MonoUs{0});          // a single LOW sample is never a press, however long ago
  CHECK(!one.pop(e));
}

TEST(input_click_vs_hold_vs_hesitation) {
  Rig r;
  r.in.configure(Key::Hash, kClick800);
  r.hold(kHash, 200); r.hold(0, 50);                        // click
  CHECK_EQ(r.count(T::Short), 1); CHECK_EQ(r.count(T::Long), 0);
  r.clear();
  r.hold(kHash, 600); r.hold(0, 50);                        // hesitation zone: nothing but Down/Up
  CHECK_EQ(r.count(T::Short), 0); CHECK_EQ(r.count(T::Long), 0); CHECK_EQ(r.count(T::Up), 1);
  r.clear();
  r.hold(kHash, 1000);                                      // hold
  CHECK_EQ(r.count(T::Long), 1);
  CHECK_EQ(r.ev[r.find(T::Long)].heldMs >= 800, true);
  r.hold(0, 50);
  CHECK_EQ(r.count(T::Short), 0); CHECK_EQ(r.count(T::Long), 1); CHECK_EQ(r.count(T::Up), 1);
}

TEST(input_hold_progress) {
  Rig r;
  r.in.configure(Key::Hash, kClick800);
  r.hold(kHash, 100);
  CHECK(!r.in.hold(MonoUs{r.tMs * 1000}).active);           // taps never flicker
  r.hold(kHash, 375);                                       // ~475 ms held
  const HoldState h = r.in.hold(MonoUs{r.tMs * 1000});
  CHECK(h.active); CHECK_EQ(h.key, Key::Hash);
  CHECK_NEAR(h.progress, 0.5f, 0.03f);
  r.in.configure(Key::Star, kOnDown);
  r.hold(0, 50);
  r.hold(kStar, 500);
  CHECK(!r.in.hold(MonoUs{r.tMs * 1000}).active);           // OnDown presses never show a hold fill
}

TEST(input_repeat_timing_and_accel) {
  Rig r;
  r.in.configure(Key::Up, KeyConfig{Trigger::OnDown, 0, true, true});
  r.hold(kUp, 3000);
  int64_t t0 = r.ev[r.find(T::Down)].tDown.v / 1000;
  int reps = 0, accel = 0;
  int64_t first = -1, prev = -1, gapSlow = -1, gapFast = -1;
  for (int i = 0; i < r.n; ++i) {
    if (r.ev[i].type != T::Repeat) continue;
    const int64_t t = t0 + r.ev[i].heldMs;
    if (first < 0) first = t - t0;
    if (prev >= 0 && t - t0 < 1500 && gapSlow < 0) gapSlow = t - prev;
    if (prev >= 0 && t - t0 > 1700) gapFast = t - prev;
    prev = t;
    ++reps;
    if (r.ev[i].step == 5) { ++accel; CHECK(r.ev[i].heldMs >= 2000); }
  }
  CHECK(first >= 400 && first <= 410);
  CHECK(gapSlow >= 120 && gapSlow <= 130);
  CHECK(gapFast >= 45 && gapFast <= 55);
  CHECK(reps > 20);
  CHECK(accel > 5);
}

TEST(input_swallow_until_release) {
  Rig r;
  r.in.configure(Key::Star, kOnDown);
  r.hold(kStar, 30);
  CHECK_EQ(r.count(T::Short), 1);                           // queued, then the gate swallows (display was off)
  // re-run with a pop in between: Down popped, then swallowed -> the queued Short disappears
  Rig s;
  s.in.configure(Key::Star, kOnDown);
  for (int i = 0; i < 6; ++i) { s.in.feed(kStar, MonoUs{i * 5000}); }
  InputEvent e;
  CHECK(s.in.pop(e)); CHECK_EQ(e.type, T::Down);
  s.in.swallowUntilRelease(kStar);
  CHECK(!s.in.pop(e));                                      // no Short
  s.tMs = 30;
  s.hold(kStar, 500); s.hold(0, 50);
  CHECK_EQ(s.n, 0);                                         // no repeat, no Up
  s.hold(kStar, 50); s.hold(0, 50);
  CHECK_EQ(s.count(T::Down), 1);                            // the next press works
  // a key already up: no effect
  Rig u;
  u.in.configure(Key::Up, kOnDown);
  u.hold(0, 20);
  u.in.swallowUntilRelease(kUp);
  u.hold(kUp, 50); u.hold(0, 50);
  CHECK_EQ(u.count(T::Down), 1);
}

TEST(input_boot_swallow_before_debounce) {
  Rig r;                                                    // boot: the key is LOW at the first sample
  r.in.configure(Key::Down, kOnDown);
  r.feed(kDn);
  r.in.swallowUntilRelease(kDn);
  r.hold(kDn, 300); r.hold(0, 50);
  CHECK_EQ(r.n, 0);
  r.hold(kDn, 50);
  CHECK_EQ(r.count(T::Down), 1);
}

TEST(input_first_key_owns_the_gesture) {
  Rig r;
  for (int k = 0; k < 4; ++k) r.in.configure((Key)k, kOnDown);
  r.hold(kHash, 50);
  r.hold(kHash | kStar, 100);                               // # owns; * ignored
  r.hold(kStar, 100);                                       // # released, * still held: still ignored
  r.hold(0, 50);
  CHECK_EQ(r.count(T::Down), 1);
  CHECK_EQ(r.ev[r.find(T::Down)].key, Key::Hash);
  r.hold(kStar, 50); r.hold(0, 50);
  CHECK_EQ(r.count(T::Down), 2);                            // all keys were up: next press works
}

TEST(input_capture_all_reports_every_key) {
  Rig r;
  r.in.setCaptureAll(true);
  r.hold(kUp, 50);
  r.hold(kUp | kDn, 50);
  r.hold(kUp | kDn | kHash | kStar, 50);
  r.hold(0, 50);
  CHECK_EQ(r.count(T::Down), 4);
  CHECK_EQ(r.count(T::Up), 4);
  r.clear();
  r.hold(kHash, 11000);                                     // no stuck rule while capturing
  CHECK_EQ(r.count(T::Stuck), 0);
}

TEST(input_latching_D25) {
  Rig r;
  r.in.configure(Key::Star, kOnDown);
  r.hold(kStar, 30);                                        // Down + Short fired on Down
  r.in.configure(Key::Star, kClick800);                     // the screen changed under the press
  r.hold(kStar, 1000);
  CHECK(!r.in.hold(MonoUs{r.tMs * 1000}).active);           // no hold fill
  r.hold(0, 50);
  CHECK_EQ(r.n, 3);
  CHECK_EQ(r.ev[0].type, T::Down); CHECK_EQ(r.ev[1].type, T::Short); CHECK_EQ(r.ev[2].type, T::Up);
  r.clear();
  r.hold(kStar, 1000); r.hold(0, 50);                       // the next press uses the new config
  CHECK_EQ(r.count(T::Long), 1);
  // OnDown + repeat press keeps repeating even if reconfigured
  Rig q;
  q.in.configure(Key::Up, KeyConfig{Trigger::OnDown, 0, true, false});
  q.hold(kUp, 30);
  q.in.configure(Key::Up, kOnDown);
  q.hold(kUp, 1000);
  CHECK(q.count(T::Repeat) > 3);
}

TEST(input_stuck_key_D26) {
  Rig r;
  for (int k = 0; k < 4; ++k) r.in.configure((Key)k, kOnDown);
  r.hold(kHash, 9990);
  CHECK_EQ(r.count(T::Stuck), 0);
  CHECK_EQ(r.in.downMask(), kHash);
  r.hold(kHash, 30);
  CHECK_EQ(r.count(T::Stuck), 1);
  CHECK_EQ(r.ev[r.find(T::Stuck)].key, Key::Hash);
  CHECK_EQ(r.in.downMask(), 0);
  CHECK_EQ(r.in.stuckMask(), kHash);
  r.clear();
  r.hold(kHash | kStar, 50); r.hold(kHash, 50);             // another key's press works
  CHECK_EQ(r.count(T::Down), 1); CHECK_EQ(r.ev[r.find(T::Down)].key, Key::Star);
  CHECK_EQ(r.count(T::Up), 1);                              // the stuck key's press never emits Up
  r.clear();
  r.hold(0, 900); r.hold(kHash, 50);                        // 0.9 s HIGH then LOW again: still stuck
  CHECK_EQ(r.count(T::Unstuck), 0);
  CHECK_EQ(r.in.stuckMask(), kHash);
  r.hold(0, 1050);                                          // 1 s HIGH: works again
  CHECK_EQ(r.count(T::Unstuck), 1);
  CHECK_EQ(r.in.stuckMask(), 0);
  r.clear();
  r.hold(kHash, 50); r.hold(0, 50);
  CHECK_EQ(r.count(T::Down), 1);
}

TEST(input_queue_overflow_drops_oldest) {
  InputLogic in;
  in.setCaptureAll(true);
  int64_t t = 0;
  for (int i = 0; i < 10; ++i) {                            // 10 presses x (Down, Short, Up) = 30 events, no pops
    for (int s = 0; s < 6; ++s, t += 5) in.feed(kUp, MonoUs{t * 1000});
    for (int s = 0; s < 6; ++s, t += 5) in.feed(0, MonoUs{t * 1000});
  }
  CHECK_EQ(in.dropped(), 14u);
  InputEvent e; int n = 0;
  while (in.pop(e)) ++n;
  CHECK_EQ(n, 16);
}
