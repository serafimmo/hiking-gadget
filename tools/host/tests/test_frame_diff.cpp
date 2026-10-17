// ui/frame_diff (SPEC §4.5): only changed tile spans; full refresh flag; no page starves.
#include <cstring>

#include "tiny_test.h"
#include "ui/frame_diff.h"

using namespace hg;

namespace {
uint32_t gSeed = 99;
uint8_t rnd() { gSeed = gSeed * 1103515245u + 12345u; return (uint8_t)(gSeed >> 16); }
void drain(FrameDiff& d, const uint8_t* f) { PageSpan s[8]; while (d.collect(f, s, 8)) {} }
}  // namespace

TEST(frame_diff_initial_full_then_clean) {
  FrameDiff d;
  uint8_t f[1024] = {};
  PageSpan s[8];
  CHECK_EQ(d.collect(f, s, 8), 8);
  for (int i = 0; i < 8; ++i) { CHECK_EQ(s[i].tx0, 0); CHECK_EQ(s[i].tw, 16); }
  CHECK_EQ(d.collect(f, s, 8), 0);
  CHECK(d.clean(f));
}

TEST(frame_diff_random_pairs_give_exact_spans) {
  int bad = 0;
  for (int round = 0; round < 200; ++round) {
    FrameDiff d;
    uint8_t a[1024], b[1024];
    for (int i = 0; i < 1024; ++i) a[i] = rnd();
    drain(d, a);
    memcpy(b, a, sizeof b);
    const int changes = rnd() % 6;
    for (int k = 0; k < changes; ++k) b[rnd() % 8 * 128 + rnd() % 128] ^= (uint8_t)(1 + rnd() % 255);
    // expected spans per page
    PageSpan s[8];
    const uint8_t n = d.collect(b, s, 8);
    int pagesChanged = 0;
    for (int p = 0; p < 8; ++p) {
      int first = -1, last = -1;
      for (int t = 0; t < 16; ++t) if (memcmp(a + p * 128 + t * 8, b + p * 128 + t * 8, 8)) { if (first < 0) first = t; last = t; }
      if (first < 0) continue;
      ++pagesChanged;
      bool found = false;
      for (int i = 0; i < n; ++i) if (s[i].page == p) found = s[i].tx0 == first && s[i].tw == last - first + 1;
      if (!found) ++bad;
    }
    if (n != pagesChanged) ++bad;
    if (!d.clean(b)) ++bad;
  }
  CHECK_EQ(bad, 0);
}

TEST(frame_diff_budget_rotates_pages) {
  FrameDiff d;
  uint8_t f[1024] = {};
  drain(d, f);
  PageSpan s[2];
  int seen[8] = {};
  for (int it = 0; it < 8; ++it) {
    for (int i = 0; i < 1024; i += 128) f[i] ^= 1;          // every page changes every frame
    const uint8_t n = d.collect(f, s, 2);
    CHECK_EQ(n, 2);
    for (int i = 0; i < n; ++i) ++seen[s[i].page];
  }
  for (int p = 0; p < 8; ++p) CHECK(seen[p] >= 1);         // no page starves under the 2-span budget
  d.invalidate();
  PageSpan all[8];
  CHECK_EQ(d.collect(f, all, 8), 8);
}
