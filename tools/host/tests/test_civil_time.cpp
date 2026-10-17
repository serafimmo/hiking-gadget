// core/civil_time (SPEC §5, Appendix B).
#include "core/civil_time.h"
#include "tiny_test.h"

using namespace hg;

TEST(civil_round_trip_1970_2100) {
  const int64_t d0 = daysFromCivil(1970, 1, 1), d1 = daysFromCivil(2100, 12, 31);
  CHECK_EQ(d0, (int64_t)0);
  int bad = 0;
  int64_t prev = d0 - 1;
  for (int y = 1970; y <= 2100; ++y)
    for (unsigned m = 1; m <= 12; ++m)
      for (unsigned d = 1; d <= daysInMonth(y, m); ++d) {
        const int64_t z = daysFromCivil(y, m, d);
        int yy; unsigned mm, dd;
        civilFromDays(z, yy, mm, dd);
        if (z != prev + 1 || yy != y || mm != m || dd != d) ++bad;
        prev = z;
      }
  CHECK_EQ(bad, 0);
  CHECK_EQ(prev, d1);
}

TEST(civil_leap_years_and_month_lengths) {
  CHECK_EQ(daysInMonth(2024, 2), 29);
  CHECK_EQ(daysInMonth(2026, 2), 28);
  CHECK_EQ(daysInMonth(2000, 2), 29);
  CHECK_EQ(daysInMonth(2100, 2), 28);
  CHECK_EQ(daysInMonth(2026, 4), 30);
  CHECK_EQ(daysInMonth(2026, 13), 0);
}

TEST(civil_weekday_and_epoch_example) {
  CHECK_EQ(weekdayFromDays(daysFromCivil(2026, 10, 11)), 6);   // Sunday
  CHECK_EQ(weekdayFromDays(0), 3);                             // 1970-01-01 Thursday
  const Civil c = civilFromEpoch(1791736927);
  CHECK_EQ(c.year, 2026); CHECK_EQ(c.month, 10); CHECK_EQ(c.day, 11);
  CHECK_EQ(c.hour, 16); CHECK_EQ(c.minute, 42); CHECK_EQ(c.second, 7); CHECK_EQ(c.weekday, 6);
  CHECK_EQ(epochFromCivil(c), (int64_t)1791736927);
  CHECK_EQ(epochFromCivil(civilFromEpoch(1735689600)), (int64_t)1735689600);
  const Civil e = civilFromEpoch(-1);
  CHECK_EQ(e.year, 1969); CHECK_EQ(e.hour, 23); CHECK_EQ(e.second, 59);
}

TEST(civil_parse_build_stamp) {
  Civil c{};
  CHECK(parseBuildStamp("Oct 11 2026", "19:30:12", c));
  CHECK_EQ(epochFromCivil(c) - 180 * 60, (int64_t)1791736212);
  CHECK_EQ(c.weekday, 6);
  CHECK(parseBuildStamp("Mar  1 2027", "00:00:00", c));   // __DATE__ pads days 1..9 with a space
  CHECK_EQ(c.day, 1); CHECK_EQ(c.month, 3);
  CHECK(!parseBuildStamp("Foo 11 2026", "19:30:12", c));
  CHECK(!parseBuildStamp("Feb 30 2026", "19:30:12", c));
  CHECK(!parseBuildStamp("Oct 11 2026", "24:00:00", c));
  CHECK(!parseBuildStamp("Oct 11 2026", "19:30", c));
}

TEST(civil_parse_local_date_time) {
  Civil c{};
  CHECK(parseLocalDateTime("2026-10-11 19:42", c));
  CHECK_EQ(c.hour, 19); CHECK_EQ(c.minute, 42); CHECK_EQ(c.second, 0);
  CHECK(parseLocalDateTime("2026-10-11T19:42:07", c));
  CHECK_EQ(c.second, 7);
  CHECK(parseLocalDateTime("2024-02-29 00:00", c));
  CHECK(!parseLocalDateTime("2026-02-29 00:00", c));
  CHECK(!parseLocalDateTime("2026-10-11 19:42:7", c));
  CHECK(!parseLocalDateTime("2026-10-11 19:42 ", c));
  CHECK(!parseLocalDateTime("2026-1-11 19:42", c));
  CHECK(!parseLocalDateTime("", c));
}

TEST(civil_parse_unix_ms) {
  int64_t ms = 0;
  CHECK(parseUnixMs("1791736927", ms));      CHECK_EQ(ms, (int64_t)1791736927000);
  CHECK(parseUnixMs("1791736927.250", ms));  CHECK_EQ(ms, (int64_t)1791736927250);
  CHECK(parseUnixMs("1791736927.25", ms));   CHECK_EQ(ms, (int64_t)1791736927250);
  CHECK(parseUnixMs("1791736927.2509", ms)); CHECK_EQ(ms, (int64_t)1791736927250);
  CHECK(!parseUnixMs("1791736927.", ms));
  CHECK(!parseUnixMs("-5", ms));
  CHECK(!parseUnixMs("17917x", ms));
  CHECK(!parseUnixMs("", ms));
  CHECK(!parseUnixMs("999999999999", ms));
}
