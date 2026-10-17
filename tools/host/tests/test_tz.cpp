// core/tz: table, EU/US DST boundaries at +-1 s, gap/overlap (SPEC §5.5).
#include <cstring>

#include "core/tz.h"
#include "tiny_test.h"

using namespace hg;

namespace {
const TzDef& tz(const char* id) { return kTzTable[findTz(id)]; }
bool dstAt(const char* id, int64_t s) { bool d = false; utcOffsetMin(tz(id), s, &d); return d; }
Civil civ(int y, int mo, int d, int h, int mi) { Civil c{}; c.year = (int16_t)y; c.month = (uint8_t)mo; c.day = (uint8_t)d;
                                                  c.hour = (uint8_t)h; c.minute = (uint8_t)mi; return c; }
}  // namespace

TEST(tz_table_entries) {
  CHECK_EQ(findTz("MSK"), (int)kTzDefault);
  CHECK_EQ(findTz("msk"), (int)kTzDefault);
  CHECK_EQ(findTz("XXX"), -1);
  CHECK_EQ(findTz(nullptr), -1);
  for (int i = 0; i < kTzCount; ++i) {
    const TzDef& t = kTzTable[i];
    CHECK_EQ(findTz(t.id), i);
    CHECK(strlen(t.stdAbbr) > 0 && strlen(t.stdAbbr) <= 5);    // ClockView.zone is char[6]
    CHECK(strlen(t.dstAbbr) > 0 && strlen(t.dstAbbr) <= 5);
    CHECK(t.stdOffsetMin >= -720 && t.stdOffsetMin <= 840);
  }
  CHECK_EQ(utcOffsetMin(tz("DEL"), 1791736927), 330);            // IST +5:30
  CHECK_STR(tzAbbr(tz("DEL"), 1791736927), "IST");
}

TEST(tz_moscow_example) {
  const Civil c = utcToLocal(1791736927, tz("MSK"));
  CHECK_EQ(c.hour, 19); CHECK_EQ(c.minute, 42); CHECK_EQ(c.second, 7); CHECK_EQ(c.weekday, 6);
  CHECK_STR(tzAbbr(tz("MSK"), 1791736927), "MSK");
}

TEST(tz_eu_boundaries_2026_2030) {
  const int64_t start[] = {1774746000, 1806195600}, end[] = {1792890000, 1824944400};
  for (int i = 0; i < 2; ++i) {
    CHECK(!dstAt("CET", start[i] - 1)); CHECK(dstAt("CET", start[i]));
    CHECK(dstAt("CET", end[i] - 1));    CHECK(!dstAt("CET", end[i]));
    CHECK(dstAt("LON", start[i]));      CHECK(!dstAt("LON", end[i]));
    CHECK(dstAt("EET", start[i]));      CHECK(!dstAt("EET", start[i] - 1));
  }
  // 2028-2030: the switch is always a Sunday 01:00 UTC, and exactly one switch each way per year.
  for (int y = 2028; y <= 2030; ++y) {
    int on = 0, off = 0;
    bool prev = dstAt("CET", daysFromCivil(y, 1, 1) * 86400);
    for (int64_t s = daysFromCivil(y, 1, 1) * 86400; s < daysFromCivil(y + 1, 1, 1) * 86400; s += 3600) {
      const bool d = dstAt("CET", s);
      if (d != prev) {
        const Civil c = civilFromEpoch(s);
        CHECK_EQ(c.weekday, 6); CHECK_EQ(c.hour, 1);
        (d ? on : off)++;
      }
      prev = d;
    }
    CHECK_EQ(on, 1); CHECK_EQ(off, 1);
  }
  CHECK_STR(tzAbbr(tz("CET"), 1774746000), "CEST");
  CHECK_STR(tzAbbr(tz("LON"), 1774746000 - 1), "GMT");
}

TEST(tz_us_boundaries_2026_2030) {
  const int64_t start[] = {1772953200, 1805007600}, end[] = {1793512800, 1825567200};
  for (int i = 0; i < 2; ++i) {
    CHECK(!dstAt("NYC", start[i] - 1)); CHECK(dstAt("NYC", start[i]));
    CHECK(dstAt("NYC", end[i] - 1));    CHECK(!dstAt("NYC", end[i]));
    CHECK(!dstAt("LAX", start[i]));     CHECK(dstAt("LAX", start[i] + 3 * 3600));   // LA switches 3 h later
  }
  for (int y = 2028; y <= 2030; ++y) {
    int on = 0, off = 0;
    bool prev = false;
    for (int64_t s = daysFromCivil(y, 1, 1) * 86400; s < daysFromCivil(y + 1, 1, 1) * 86400; s += 3600) {
      const bool d = dstAt("NYC", s);
      if (d != prev) {
        const Civil local = civilFromEpoch(s + (d ? -300 : -240) * 60);   // local time just before the switch
        CHECK_EQ(local.weekday, 6); CHECK_EQ(local.hour, 2);
        (d ? on : off)++;
      }
      prev = d;
    }
    CHECK_EQ(on, 1); CHECK_EQ(off, 1);
  }
  CHECK_STR(tzAbbr(tz("NYC"), 1772953200), "EDT");
}

TEST(tz_local_to_utc_gap_and_overlap) {
  // Normal times round-trip in every zone.
  for (int i = 0; i < kTzCount; ++i) {
    const int64_t u = 1791736927;
    CHECK_EQ(localToUtc(utcToLocal(u, kTzTable[i]), kTzTable[i]), u);
    CHECK_EQ(localToUtc(utcToLocal(u + 120 * 86400, kTzTable[i]), kTzTable[i]), u + 120 * 86400);
  }
  // NYC spring-forward gap 2026-03-08 02:30 -> shown 03:30 EDT
  const int64_t g = localToUtc(civ(2026, 3, 8, 2, 30), tz("NYC"));
  CHECK_EQ(g, (int64_t)1772953200 + 30 * 60);
  CHECK_EQ(utcToLocal(g, tz("NYC")).hour, 3);
  // NYC autumn overlap 2026-11-01 01:30 -> the earlier (EDT) instant 05:30Z
  CHECK_EQ(localToUtc(civ(2026, 11, 1, 1, 30), tz("NYC")), (int64_t)1793512800 - 30 * 60);
  // Berlin gap 2026-03-29 02:30 -> 03:30 CEST; overlap 2026-10-25 02:30 -> 00:30Z
  const int64_t bg = localToUtc(civ(2026, 3, 29, 2, 30), tz("CET"));
  CHECK_EQ(utcToLocal(bg, tz("CET")).hour, 3);
  CHECK_EQ(localToUtc(civ(2026, 10, 25, 2, 30), tz("CET")), (int64_t)1792890000 - 30 * 60);
}
