#pragma once
#include "types.h"
namespace hg {
struct Civil { int16_t year; uint8_t month, day, hour, minute, second, weekday; };  // weekday 0=Mon..6=Sun
int64_t daysFromCivil(int y, unsigned m, unsigned d);   // Hinnant
void    civilFromDays(int64_t z, int& y, unsigned& m, unsigned& d);
uint8_t weekdayFromDays(int64_t z);
uint8_t daysInMonth(int y, unsigned m);
Civil   civilFromEpoch(int64_t epochS);                 // fields as UTC
int64_t epochFromCivil(const Civil& c);                 // fields as UTC (weekday ignored)
bool parseBuildStamp(const char* date, const char* time, Civil& out);   // "Oct 11 2026", "19:30:12"
bool parseLocalDateTime(const char* s, Civil& out);     // "YYYY-MM-DD HH:MM[:SS]" or with 'T'
bool parseUnixMs(const char* s, int64_t& ms);           // "1791736927" or "1791736927.250"
}  // namespace hg
