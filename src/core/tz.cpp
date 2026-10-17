#include "tz.h"

namespace hg {

// Stable ids are stored in NVS (§8.2), so rows may be reordered. ASCII only (D29).
const TzDef kTzTable[kTzCount] = {
    {"LAX", "PST", "PDT", "LOS ANGELES", "US DST", -480, DstRule::US},
    {"NYC", "EST", "EDT", "NEW YORK", "US DST", -300, DstRule::US},
    {"UTC", "UTC", "UTC", "UTC", "No DST", 0, DstRule::None},
    {"LON", "GMT", "BST", "LONDON", "EU DST", 0, DstRule::EU},
    {"CET", "CET", "CEST", "BERLIN", "Paris, Rome", 60, DstRule::EU},
    {"EET", "EET", "EEST", "HELSINKI", "Kyiv, Riga", 120, DstRule::EU},
    {"KGD", "EET", "EET", "KALININGRAD", "No DST", 120, DstRule::None},
    {"MSK", "MSK", "MSK", "MOSCOW", "Minsk, Istanbul", 180, DstRule::None},
    {"SAM", "SAMT", "SAMT", "SAMARA", "Dubai, Baku", 240, DstRule::None},
    {"YEK", "YEKT", "YEKT", "YEKATERINBURG", "Tashkent, Almaty", 300, DstRule::None},
    {"DEL", "IST", "IST", "DELHI", "India", 330, DstRule::None},
    {"OMS", "OMST", "OMST", "OMSK", "Bishkek", 360, DstRule::None},
    {"NSK", "+07", "+07", "NOVOSIBIRSK", "Krasnoyarsk", 420, DstRule::None},
    {"IRK", "+08", "+08", "IRKUTSK", "Beijing, Singapore", 480, DstRule::None},
    {"YAK", "+09", "+09", "YAKUTSK", "Tokyo, Seoul", 540, DstRule::None},
    {"VLA", "+10", "+10", "VLADIVOSTOK", "No DST", 600, DstRule::None},
    {"MAG", "+11", "+11", "MAGADAN", "Sakhalin", 660, DstRule::None},
    {"PET", "+12", "+12", "KAMCHATKA", "No DST", 720, DstRule::None},
};

namespace {
constexpr uint8_t kSunday = 6;   // weekday 0 = Monday

int lastSunday(int y, unsigned m) {
  const int dim = daysInMonth(y, m);
  const int wd = weekdayFromDays(daysFromCivil(y, m, (unsigned)dim));
  return dim - (wd - kSunday + 7) % 7;
}

int nthSunday(int y, unsigned m, int n) {
  const int wd = weekdayFromDays(daysFromCivil(y, m, 1));
  return 1 + (kSunday - wd + 7) % 7 + 7 * (n - 1);
}

int64_t utcAt(int y, unsigned m, int d, int minutesUtc) { return daysFromCivil(y, m, (unsigned)d) * 86400 + minutesUtc * 60; }

bool inDst(const TzDef& tz, int64_t utcS) {
  if (tz.dst == DstRule::None) return false;
  // The year of the instant in standard local time decides which year's rule applies (both rules sit far from New Year).
  const int y = civilFromEpoch(utcS + tz.stdOffsetMin * 60).year;
  int64_t start, end;
  if (tz.dst == DstRule::EU) {   // last Sunday of March 01:00 UTC .. last Sunday of October 01:00 UTC
    start = utcAt(y, 3, lastSunday(y, 3), 60);
    end = utcAt(y, 10, lastSunday(y, 10), 60);
  } else {                        // second Sunday of March 02:00 standard .. first Sunday of November 02:00 daylight
    start = utcAt(y, 3, nthSunday(y, 3, 2), 120 - tz.stdOffsetMin);
    end = utcAt(y, 11, nthSunday(y, 11, 1), 120 - (tz.stdOffsetMin + 60));
  }
  return utcS >= start && utcS < end;
}

char upper(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }
}  // namespace

int findTz(const char* id) {
  if (!id) return -1;
  for (int i = 0; i < kTzCount; ++i) {
    const char* a = kTzTable[i].id;
    const char* b = id;
    while (*a && upper(*b) == *a) { ++a; ++b; }
    if (!*a && !*b) return i;
  }
  return -1;
}

int16_t utcOffsetMin(const TzDef& tz, int64_t utcS, bool* isDst) {
  const bool dst = inDst(tz, utcS);
  if (isDst) *isDst = dst;
  return (int16_t)(tz.stdOffsetMin + (dst ? 60 : 0));
}

Civil utcToLocal(int64_t utcS, const TzDef& tz) { return civilFromEpoch(utcS + utcOffsetMin(tz, utcS) * 60); }

int64_t localToUtc(const Civil& local, const TzDef& tz) {
  const int64_t asUtc = epochFromCivil(local);
  const int64_t uStd = asUtc - tz.stdOffsetMin * 60;
  if (tz.dst == DstRule::None) return uStd;
  const int64_t uDst = uStd - 3600;
  const bool dstOk = utcOffsetMin(tz, uDst) == tz.stdOffsetMin + 60;
  const bool stdOk = utcOffsetMin(tz, uStd) == tz.stdOffsetMin;
  if (dstOk) return uDst;      // valid daylight time, or the earlier instant of an autumn overlap
  if (stdOk) return uStd;
  return uStd;                 // spring-forward gap: read as standard time = shown 60 min later
}

const char* tzAbbr(const TzDef& tz, int64_t utcS) { return inDst(tz, utcS) ? tz.dstAbbr : tz.stdAbbr; }

}  // namespace hg
