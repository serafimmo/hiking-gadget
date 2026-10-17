#include "civil_time.h"

namespace hg {
namespace {
int64_t floorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0))); }

bool isDigit(char c) { return c >= '0' && c <= '9'; }

// Reads exactly n digits.
bool digits(const char*& s, int n, int& out) {
  out = 0;
  for (int i = 0; i < n; ++i) {
    if (!isDigit(s[i])) return false;
    out = out * 10 + (s[i] - '0');
  }
  s += n;
  return true;
}

bool fill(Civil& c, int y, int mo, int d, int h, int mi, int se) {
  if (y < 1970 || y > 2199 || mo < 1 || mo > 12 || d < 1 || d > daysInMonth(y, (unsigned)mo) || h > 23 || mi > 59 ||
      se > 59 || h < 0 || mi < 0 || se < 0)
    return false;
  c.year = (int16_t)y; c.month = (uint8_t)mo; c.day = (uint8_t)d;
  c.hour = (uint8_t)h; c.minute = (uint8_t)mi; c.second = (uint8_t)se;
  c.weekday = weekdayFromDays(daysFromCivil(y, (unsigned)mo, (unsigned)d));
  return true;
}
}  // namespace

int64_t daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int64_t era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int64_t)doe - 719468;
}

void civilFromDays(int64_t z, int& y, unsigned& m, unsigned& d) {
  z += 719468;
  const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = (unsigned)(z - era * 146097);
  const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = (int)(yoe + era * 400) + (m <= 2);
}

uint8_t weekdayFromDays(int64_t z) {   // 1970-01-01 was a Thursday (3 with 0 = Monday)
  int64_t w = (z + 3) % 7;
  return (uint8_t)(w < 0 ? w + 7 : w);
}

uint8_t daysInMonth(int y, unsigned m) {
  static const uint8_t kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m < 1 || m > 12) return 0;
  const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
  return (uint8_t)(kDays[m - 1] + (m == 2 && leap ? 1 : 0));
}

Civil civilFromEpoch(int64_t epochS) {
  const int64_t days = floorDiv(epochS, 86400);
  const int64_t sod = epochS - days * 86400;
  int y; unsigned m, d;
  civilFromDays(days, y, m, d);
  Civil c{};
  c.year = (int16_t)y; c.month = (uint8_t)m; c.day = (uint8_t)d;
  c.hour = (uint8_t)(sod / 3600); c.minute = (uint8_t)(sod / 60 % 60); c.second = (uint8_t)(sod % 60);
  c.weekday = weekdayFromDays(days);
  return c;
}

int64_t epochFromCivil(const Civil& c) {
  return daysFromCivil(c.year, c.month, c.day) * 86400 + c.hour * 3600 + c.minute * 60 + c.second;
}

bool parseBuildStamp(const char* date, const char* time, Civil& out) {
  static const char kMon[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
  if (!date || !time) return false;
  int mo = 0;
  for (int i = 0; i < 12; ++i)
    if (date[0] == kMon[i * 3] && date[1] == kMon[i * 3 + 1] && date[2] == kMon[i * 3 + 2]) mo = i + 1;
  if (!mo || date[3] != ' ') return false;
  const char* s = date + 4;
  int d = 0;
  if (*s == ' ') { ++s; if (!digits(s, 1, d)) return false; }      // __DATE__ pads days 1..9 with a space
  else if (!digits(s, 2, d)) return false;
  int y, h, mi, se;
  if (*s++ != ' ' || !digits(s, 4, y) || *s) return false;
  const char* t = time;
  if (!digits(t, 2, h) || *t++ != ':' || !digits(t, 2, mi) || *t++ != ':' || !digits(t, 2, se) || *t) return false;
  return fill(out, y, mo, d, h, mi, se);
}

bool parseLocalDateTime(const char* s, Civil& out) {
  if (!s) return false;
  int y, mo, d, h, mi, se = 0;
  if (!digits(s, 4, y) || *s++ != '-' || !digits(s, 2, mo) || *s++ != '-' || !digits(s, 2, d)) return false;
  if (*s != ' ' && *s != 'T' && *s != 't') return false;
  ++s;
  if (!digits(s, 2, h) || *s++ != ':' || !digits(s, 2, mi)) return false;
  if (*s == ':') { ++s; if (!digits(s, 2, se)) return false; }
  if (*s) return false;
  return fill(out, y, mo, d, h, mi, se);
}

bool parseUnixMs(const char* s, int64_t& ms) {
  if (!s || !isDigit(*s)) return false;
  int64_t sec = 0;
  int n = 0;
  for (; isDigit(*s); ++s) {
    if (++n > 11) return false;                       // > 11 digits is past year 5000: garbage
    sec = sec * 10 + (*s - '0');
  }
  int frac = 0;
  if (*s == '.') {
    ++s;
    int k = 0;
    for (; isDigit(*s); ++s, ++k) {
      if (k >= 9) return false;
      if (k < 3) frac = frac * 10 + (*s - '0');
    }
    if (k == 0) return false;
    for (; k < 3; ++k) frac *= 10;
  }
  if (*s) return false;
  ms = sec * 1000 + frac;
  return true;
}

}  // namespace hg
