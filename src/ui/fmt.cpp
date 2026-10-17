#include "fmt.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../config/version.h"
#include "strings.h"

namespace hg {
namespace {
int64_t absll(int64_t v) { return v < 0 ? -v : v; }
void copy(char* b, size_t n, const char* s) {
  if (!n) return;
  size_t i = 0;
  for (; s && s[i] && i + 1 < n; ++i) b[i] = s[i];
  b[i] = 0;
}
void withArg(char* b, size_t n, uint16_t id, const char* arg) { snprintf(b, n, str(id), arg); }
}  // namespace

void fmtClock(char* b, size_t n, int h, int m) { snprintf(b, n, "%02d:%02d", h % 100, m % 100); }
void fmtClockSec(char* b, size_t n, int h, int m, int s) { snprintf(b, n, "%02d:%02d:%02d", h % 100, m % 100, s % 100); }

void fmtMmSs(char* b, size_t n, int64_t s) {
  if (s < 0) s = 0;
  snprintf(b, n, "%02d:%02d", (int)(s / 60 % 1000), (int)(s % 60));
}

void fmtHMmSs(char* b, size_t n, int64_t s) {
  if (s < 0) s = 0;
  snprintf(b, n, "%d:%02d:%02d", (int)(s / 3600 % 1000), (int)(s / 60 % 60), (int)(s % 60));
}

void fmtLapTime(char* b, size_t n, int64_t us) {
  if (us < 0) us = 0;
  const int64_t s = us / kUsPerS;
  if (s < 3600) snprintf(b, n, "%02d:%02d.%02d", (int)(s / 60), (int)(s % 60), (int)(us / 10000 % 100));
  else if (s < 36000) snprintf(b, n, "%d:%02d:%02d", (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
  else snprintf(b, n, "%02d:%02d:%02d", (int)(s / 3600 % 100), (int)(s / 60 % 60), (int)(s % 60));
}

void fmtLapDelta(char* b, size_t n, int64_t us) {
  const char sign = us < 0 ? '-' : '+';
  const int64_t a = absll(us), s = a / kUsPerS;
  if (s < 60) snprintf(b, n, "%c%d.%02d", sign, (int)s, (int)(a / 10000 % 100));
  else if (s < 3600) snprintf(b, n, "%c%d:%02d", sign, (int)(s / 60), (int)(s % 60));
  else snprintf(b, n, "%c%d:%02d:%02d", sign, (int)(s / 3600 % 100), (int)(s / 60 % 60), (int)(s % 60));
}

void fmtTimerTotal(char* b, size_t n, int64_t us) {
  const int64_t min = (us + 59 * kUsPerS) / (60 * kUsPerS);
  if (min < 100) snprintf(b, n, "%d MIN", (int)min);
  else snprintf(b, n, "%dH %02d", (int)(min / 60), (int)(min % 60));
}

void fmtTimerTotalLower(char* b, size_t n, int64_t us) {
  const int64_t min = (us + 59 * kUsPerS) / (60 * kUsPerS);
  if (min < 100) snprintf(b, n, "%d min", (int)min);
  else snprintf(b, n, "%dh %02d", (int)(min / 60), (int)(min % 60));
}

void fmtTimerChip(char* b, size_t n, int64_t remainS) {
  if (remainS < 0) remainS = 0;
  if (remainS < 60) snprintf(b, n, "%dS", (int)remainS);
  else if (remainS < 3600) snprintf(b, n, "%dM", (int)((remainS + 59) / 60));
  else snprintf(b, n, "%dH", (int)((remainS + 1800) / 3600));
}

void fmtAgo(char* b, size_t n, int64_t s) {
  if (s < 60) copy(b, n, "now");
  else if (s < 3600) snprintf(b, n, "%dm", (int)(s / 60));
  else if (s < 86400) snprintf(b, n, "%dh", (int)(s / 3600));
  else snprintf(b, n, "%dd", (int)(s / 86400 % 100000));
}

void fmtTemp(char* b, size_t n, float c, bool fahrenheit) {
  const float v = fahrenheit ? c * 9.0f / 5.0f + 32.0f : c;
  const int t = (int)lroundf(v * 10.0f);
  snprintf(b, n, "%s%d.%d", t < 0 ? "-" : "", (t < 0 ? -t : t) / 10, (t < 0 ? -t : t) % 10);
}

void fmtTenthsSigned(char* b, size_t n, int tenths) {
  const int a = tenths < 0 ? -tenths : tenths;
  snprintf(b, n, "%s%d.%d", tenths > 0 ? "+" : tenths < 0 ? "-" : "", a / 10, a % 10);
}

void fmtIntSigned(char* b, size_t n, int v) { snprintf(b, n, v > 0 ? "+%d" : "%d", v); }

const char* weekdayAbbr(uint8_t wd) {
  static const char* const k[7] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
  return wd < 7 ? k[wd] : "???";
}

const char* monthAbbr(uint8_t m) {
  static const char* const k[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  return m >= 1 && m <= 12 ? k[m - 1] : "???";
}

void fmtDateShort(char* b, size_t n, const Civil& c, bool weekday) {
  if (weekday) snprintf(b, n, "%s %d %s", weekdayAbbr(c.weekday), c.day, monthAbbr(c.month));
  else snprintf(b, n, "%d %s", c.day, monthAbbr(c.month));
}

void fmtDateLong(char* b, size_t n, const Civil& c) {
  snprintf(b, n, "%s %02d.%02d.%04d", weekdayAbbr(c.weekday), c.day, c.month, c.year);
}

void fmtUtcOffset(char* b, size_t n, int16_t offsetMin) {
  if (offsetMin == 0) { copy(b, n, "UTC"); return; }
  const int a = offsetMin < 0 ? -offsetMin : offsetMin;
  if (a % 60) snprintf(b, n, "UTC%c%d:%02d", offsetMin < 0 ? '-' : '+', a / 60, a % 60);
  else snprintf(b, n, "UTC%c%d", offsetMin < 0 ? '-' : '+', a / 60);
}

void tzTitleCase(char* b, size_t n, const TzDef& tz) {
  copy(b, n, tz.title);
  bool start = true;
  for (char* p = b; *p; ++p) {
    if (*p >= 'A' && *p <= 'Z' && !start) *p = (char)(*p + 32);
    start = *p == ' ' || *p == '-';
  }
}

void toUpper(char* s) {
  for (; s && *s; ++s)
    if (*s >= 'a' && *s <= 'z') *s = (char)(*s - 32);
}

uint16_t settingLabel(SettingId id) {
  static const uint16_t k[(int)SettingId::Count] = {
      S_SET_TIMEZONE, S_SET_SETTIME, S_SET_SCREENOFF, S_SET_BRIGHTNESS, S_SET_ANIMATIONS, S_SET_AUTOOFF,
      S_SET_LIGHTSLEEP, S_SET_TEMPUNIT, S_SET_TEMPOFFSET, S_SET_HEATER, S_SET_HEATERPOWER, S_SET_DECLINATION,
      S_SET_MOUNT, S_SET_NEEDLE, S_SET_CALIBRATE, S_SET_DIAGNOSTICS, S_SET_POWEROFF, S_SET_RESET, S_SET_VERSION};
  return (int)id < (int)SettingId::Count ? k[(int)id] : (uint16_t)S_NONE;
}

uint16_t sectionTitle(Section s) {
  static const uint16_t k[] = {S_SEC_CLOCK, S_SEC_DISPLAY, S_SEC_POWER, S_SEC_SENSOR, S_SEC_COMPASS, S_SEC_SYSTEM};
  return (int)s < 6 ? k[(int)s] : (uint16_t)S_NONE;
}

void formatSettingValue(SettingId id, int16_t v, ValueStyle st, const FormatCtx& ctx, char* b, size_t n) {
  if (!n) return;
  b[0] = 0;
  const bool picker = st == ValueStyle::Picker;
  const int16_t o = optionValue(id, v);
  switch (id) {
    case SettingId::TimeZone: {
      const TzDef& tz = kTzTable[sanitize(id, v)];
      if (st == ValueStyle::Picker) copy(b, n, tz.title);
      else if (st == ValueStyle::Neighbour) {
        char off[12];
        fmtUtcOffset(off, sizeof off, utcOffsetMin(tz, ctx.utcS));
        if (tz.stdOffsetMin == 0 && tz.dst == DstRule::None) copy(b, n, "UTC");
        else { char t[24]; tzTitleCase(t, sizeof t, tz); snprintf(b, n, "%s %s", t, off); }
      } else copy(b, n, tzAbbr(tz, ctx.utcS));
      return;
    }
    case SettingId::ScreenOff:
      if (o == 0) copy(b, n, str(S_V_NEVER));
      else if (o < 60) snprintf(b, n, "%d s", o);
      else snprintf(b, n, "%d min", o / 60);
      return;
    case SettingId::AutoOff:
      if (o == 0) copy(b, n, str(S_V_NEVER));
      else snprintf(b, n, "%d min", o);
      return;
    case SettingId::Brightness:
    case SettingId::HeaterPower:
      if (id == SettingId::HeaterPower && editable(id, ctx.sht, ctx.mag) != NotEditable::No) { copy(b, n, str(S_V_NA)); return; }
      if (id == SettingId::HeaterPower) {
        static const char* const mw[3] = {"20 mW", "110 mW", "200 mW"};
        if (picker) snprintf(b, n, "%s %s", str(o == 0 ? S_V_LOW : o == 1 ? S_V_MID : S_V_HIGH), mw[o % 3]);
        else copy(b, n, mw[o % 3]);
        return;
      }
      copy(b, n, str(o == 0 ? S_V_LOW : o == 1 ? S_V_MID : S_V_HIGH));
      return;
    case SettingId::Animations:
    case SettingId::LightSleep:
      copy(b, n, str(o ? S_V_ON : S_V_OFF));
      return;
    case SettingId::TempUnit:
      copy(b, n, o ? "`F" : "`C");
      return;
    case SettingId::TempOffset: {
      char t[12];
      fmtTenthsSigned(t, sizeof t, v);
      snprintf(b, n, "%s`C", t);
      return;
    }
    case SettingId::Heater: {
      if (editable(id, ctx.sht, ctx.mag) != NotEditable::No) { copy(b, n, str(S_V_NA)); return; }
      static const int kMin[6] = {0, 0, 5, 10, 30, 60};
      if (o == 0) copy(b, n, str(S_V_OFF));
      else if (o == 1) copy(b, n, str(S_V_AUTO));
      else if (picker) snprintf(b, n, "EVERY %d MIN", kMin[o % 6]);
      else snprintf(b, n, "%d min", kMin[o % 6]);
      if (picker) toUpper(b);
      return;
    }
    case SettingId::Declination: {
      char t[8];
      fmtIntSigned(t, sizeof t, v);
      snprintf(b, n, "%s`", t);
      return;
    }
    case SettingId::Mount: {
      const Mount m = mountFromSetting((uint8_t)o);
      snprintf(b, n, "%d` %s", m.rot * 90, str(m.chipDown ? S_V_DOWN : S_V_UP));
      if (picker) toUpper(b);
      return;
    }
    case SettingId::NeedleSpeed:
      copy(b, n, str(o == 0 ? S_V_CALM : o == 1 ? S_V_NORMAL : S_V_FAST));
      return;
    case SettingId::Calibrate: {
      if (editable(id, ctx.sht, ctx.mag) != NotEditable::No) { copy(b, n, str(S_V_NA)); return; }
      if (ctx.calPct < 0) { copy(b, n, str(S_V_NONE)); return; }
      char p[8];
      snprintf(p, sizeof p, "%d%%", ctx.calPct);
      withArg(b, n, ctx.calGrade == 0 ? S_V_GOOD : S_V_OK, p);
      return;
    }
    case SettingId::Version:
      copy(b, n, kFwVersion);
      return;
    default:
      return;   // Set time, Diagnostics, Power off, Reset settings: action rows have no value text
  }
}

void formatSettingSubLabel(SettingId id, int16_t v, const FormatCtx& ctx, char* b, size_t n) {
  if (!n) return;
  b[0] = 0;
  switch (id) {
    case SettingId::TimeZone: {
      const TzDef& tz = kTzTable[sanitize(id, v)];
      char off[12];
      fmtUtcOffset(off, sizeof off, utcOffsetMin(tz, ctx.utcS));
      if (tz.stdOffsetMin == 0 && tz.dst == DstRule::None) copy(b, n, tz.note);
      else snprintf(b, n, "%s %s", off, tz.note);
      return;
    }
    case SettingId::TempOffset: {
      if (!ctx.haveTemp) { copy(b, n, str(S_SUB_NO_SENSOR)); return; }
      char t[12], u[16];
      fmtTemp(t, sizeof t, ctx.liveTempC + v / 10.0f, false);
      snprintf(u, sizeof u, "%s`C", t);
      withArg(b, n, S_SUB_NOW_TEMP, u);
      return;
    }
    case SettingId::Heater:
      if (ctx.sht == ShtChip::Sht3x) copy(b, n, str(S_SUB_HEAT_SHT3X));
      else if (ctx.sht != ShtChip::Sht4x) copy(b, n, str(S_SUB_HEAT_NONE));
      else copy(b, n, str(optionValue(id, v) == 0 ? S_SUB_HEAT_OFF : optionValue(id, v) == 1 ? S_SUB_HEAT_AUTO : S_SUB_HEAT_EVERY));
      return;
    case SettingId::HeaterPower:
      copy(b, n, str(ctx.sht == ShtChip::Sht3x ? S_SUB_HEAT_SHT3X : ctx.sht != ShtChip::Sht4x ? S_SUB_HEAT_NONE : S_SUB_HPW));
      return;
    case SettingId::Declination: copy(b, n, str(S_SUB_DECL)); return;
    case SettingId::Mount: {
      if (!ctx.haveHeading) { copy(b, n, str(S_SUB_NO_COMPASS)); return; }
      char h[8];
      snprintf(h, sizeof h, "%03d`", (int)lroundf(wrap360(ctx.liveHeadingDeg)) % 360);
      withArg(b, n, S_SUB_HEADING, h);
      return;
    }
    case SettingId::ScreenOff:   copy(b, n, str(S_SUB_SCREENOFF)); return;
    case SettingId::AutoOff:     copy(b, n, str(S_SUB_AUTOOFF)); return;
    case SettingId::LightSleep:  copy(b, n, str(S_SUB_LIGHTSLEEP)); return;
    case SettingId::Brightness:  copy(b, n, str(S_SUB_BRIGHT)); return;
    case SettingId::Animations:  copy(b, n, str(S_SUB_ANIM)); return;
    case SettingId::TempUnit:    copy(b, n, str(S_SUB_TEMPUNIT)); return;
    case SettingId::NeedleSpeed: copy(b, n, str(S_SUB_NEEDLE)); return;
    default: return;
  }
}

}  // namespace hg
