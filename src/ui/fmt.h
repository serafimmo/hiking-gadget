#pragma once
// Text formatting for the UI: times, durations, temperatures, dates, settings values (SPEC §3.8, §4.10, §5.5).
// Everything is printable ASCII; '`' (kDegChar) is the degree sign drawn by Canvas.
#include "../core/civil_time.h"
#include "../core/settings.h"
#include "../core/compass_math.h"
#include "../core/tz.h"
#include "view_types.h"

namespace hg {
enum class ValueStyle : uint8_t { List, Picker, Cover, Neighbour };   // Neighbour: items above/below in a picker

// Clock and durations (seconds are whole; callers ceil/floor as their screen needs).
void fmtClock(char* b, size_t n, int h, int m);                      // "14:05"
void fmtClockSec(char* b, size_t n, int h, int m, int s);            // "14:05:27"
void fmtMmSs(char* b, size_t n, int64_t s);                          // "04:37" (minutes not capped)
void fmtHMmSs(char* b, size_t n, int64_t s);                         // "1:02:13", "10:00:00"
void fmtLapTime(char* b, size_t n, int64_t us);                      // MM:SS.cc < 1 h; H:MM:SS < 10 h; HH:MM:SS
void fmtLapDelta(char* b, size_t n, int64_t us);                     // +S.cc < 60 s; +M:SS < 1 h; +H:MM:SS (sign always)
void fmtTimerTotal(char* b, size_t n, int64_t us);                   // "5 MIN" (< 100 min), "1H 40" .. "10H 00"
void fmtTimerTotalLower(char* b, size_t n, int64_t us);              // "5 min", "1h 40" (Body sentences)
void fmtTimerChip(char* b, size_t n, int64_t remainS);               // "42S", "5M", "1H".."10H" (<= 3 chars)
void fmtAgo(char* b, size_t n, int64_t s);                           // "now", "5m", "2h", "3d"
// Numbers
void fmtTemp(char* b, size_t n, float c, bool fahrenheit);           // "23.4", "-12.3" (one decimal)
void fmtTenthsSigned(char* b, size_t n, int tenths);                 // "+0.3", "-1.5", "0.0"
void fmtIntSigned(char* b, size_t n, int v);                         // "+12", "-5", "0"
// Dates
const char* weekdayAbbr(uint8_t wd);                                 // "MON".."SUN" (0 = Monday)
const char* monthAbbr(uint8_t m);                                    // "JAN".."DEC"
void fmtDateShort(char* b, size_t n, const Civil& c, bool weekday);  // "SUN 11 OCT" / "11 OCT"
void fmtDateLong(char* b, size_t n, const Civil& c);                 // "SUN 11.10.2026"
void fmtUtcOffset(char* b, size_t n, int16_t offsetMin);             // "UTC+3", "UTC+5:30", "UTC-8", "UTC"
void tzTitleCase(char* b, size_t n, const TzDef& tz);                // "Los Angeles" (from the caps title)
void toUpper(char* s);
// Settings (§8.1, §3.8.5)
uint16_t settingLabel(SettingId id);                                 // StrId of the row label ("Time zone")
uint16_t sectionTitle(Section s);                                    // StrId of the section header ("CLOCK")
void formatSettingValue(SettingId id, int16_t v, ValueStyle st, const FormatCtx& ctx, char* b, size_t n);
void formatSettingSubLabel(SettingId id, int16_t v, const FormatCtx& ctx, char* b, size_t n);
}  // namespace hg
