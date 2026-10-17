#include "settings.h"

#include <string.h>

namespace hg {
namespace {
const int16_t kScreenOffS[] = {10, 20, 30, 60, 120, 300, 0};   // 0 = never
const int16_t kAutoOffMin[] = {5, 10, 30, 60, 0};              // 0 = never
const int16_t kOnOff[] = {1, 0};                               // index 0 = On
const int16_t kThree[] = {0, 1, 2};                            // Brightness, HeaterPower, NeedleSpeed
const int16_t kTwo[] = {0, 1};                                 // TempUnit C/F
const int16_t kHeater[] = {0, 1, 2, 3, 4, 5};                  // HeaterMode
const int16_t kMount[] = {0, 1, 2, 3, 4, 5, 6, 7};
#define N(a) (uint8_t)(sizeof(a) / sizeof(a[0]))
using K = SettingKind;
using S = Section;
//                       id                          section     kind          nvs      def  min  max step options    n
const SettingDef kDefs[(int)SettingId::Count] = {
    {SettingId::TimeZone,      S::Clock,   K::Enum,       "tz",    7,   0,  17, 1, nullptr,     18,          false},
    {SettingId::SetTime,       S::Clock,   K::Action,     nullptr, 0,   0,   0, 0, nullptr,     0,           false},
    {SettingId::ScreenOff,     S::Display, K::Enum,       "scr",   2,   0,   6, 1, kScreenOffS, N(kScreenOffS), false},
    {SettingId::Brightness,    S::Display, K::Enum,       "bri",   1,   0,   2, 1, kThree,      N(kThree),   false},
    {SettingId::Animations,    S::Display, K::Enum,       "anim",  0,   0,   1, 1, kOnOff,      N(kOnOff),   false},
    {SettingId::AutoOff,       S::Power,   K::Enum,       "aoff",  2,   0,   4, 1, kAutoOffMin, N(kAutoOffMin), false},
    {SettingId::LightSleep,    S::Power,   K::Enum,       "lslp",  0,   0,   1, 1, kOnOff,      N(kOnOff),   false},
    {SettingId::TempUnit,      S::Sensor,  K::Enum,       "tunit", 0,   0,   1, 1, kTwo,        N(kTwo),     false},
    {SettingId::TempOffset,    S::Sensor,  K::Number,     "toff",  0, -50,  50, 1, nullptr,     0,           true},
    {SettingId::Heater,        S::Sensor,  K::Enum,       "heat",  0,   0,   5, 1, kHeater,     N(kHeater),  false},
    {SettingId::HeaterPower,   S::Sensor,  K::Enum,       "hpw",   1,   0,   2, 1, kThree,      N(kThree),   false},
    {SettingId::Declination,   S::Compass, K::Number,     "decl",  0, -30,  30, 1, nullptr,     0,           true},
    {SettingId::Mount,         S::Compass, K::Enum,       "mnt",   0,   0,   7, 1, kMount,      N(kMount),   false},
    {SettingId::NeedleSpeed,   S::Compass, K::Enum,       "ndl",   1,   0,   2, 1, kThree,      N(kThree),   false},
    {SettingId::Calibrate,     S::Compass, K::Action,     nullptr, 0,   0,   0, 0, nullptr,     0,           false},
    {SettingId::Diagnostics,   S::System,  K::Action,     nullptr, 0,   0,   0, 0, nullptr,     0,           false},
    {SettingId::PowerOff,      S::System,  K::ActionHold, nullptr, 0,   0,   0, 0, nullptr,     0,           false},
    {SettingId::ResetSettings, S::System,  K::ActionHold, nullptr, 0,   0,   0, 0, nullptr,     0,           false},
    {SettingId::Version,       S::System,  K::Info,       nullptr, 0,   0,   0, 0, nullptr,     0,           false},
};
#undef N

bool valid(SettingId id) { return (uint8_t)id < (uint8_t)SettingId::Count; }
}  // namespace

int16_t Settings::get(SettingId id) const { return valid(id) ? v[(int)id] : 0; }
void Settings::set(SettingId id, int16_t val) { if (valid(id)) v[(int)id] = val; }

const SettingDef& settingDef(SettingId id) { return kDefs[valid(id) ? (int)id : 0]; }

Settings defaultSettings() {
  Settings s{};
  for (int i = 0; i < (int)SettingId::Count; ++i) s.v[i] = kDefs[i].def;
  return s;
}

bool isValid(SettingId id, int16_t v) {
  if (!valid(id)) return false;
  const SettingDef& d = kDefs[(int)id];
  switch (d.kind) {
    case SettingKind::Enum:   return v >= d.min && v <= d.max;
    case SettingKind::Number: return v >= d.min && v <= d.max && (v - d.min) % d.step == 0;
    default:                  return v == 0;
  }
}

int16_t sanitize(SettingId id, int16_t v) { return isValid(id, v) ? v : settingDef(id).def; }

int16_t stepValue(SettingId id, int16_t cur, int dir, uint8_t step) {
  if (!valid(id)) return cur;
  const SettingDef& d = kDefs[(int)id];
  if (d.kind != SettingKind::Enum && d.kind != SettingKind::Number) return cur;
  const int sign = dir > 0 ? 1 : dir < 0 ? -1 : 0;
  const int delta = d.kind == SettingKind::Enum ? sign : sign * d.step * (d.accel && step ? step : 1);
  int nv = sanitize(id, cur) + delta;
  if (nv < d.min) nv = d.min;
  if (nv > d.max) nv = d.max;
  return (int16_t)nv;
}

NotEditable editable(SettingId id, ShtChip sht, MagChip mag) {
  switch (id) {
    case SettingId::Heater:
    case SettingId::HeaterPower:
      if (sht == ShtChip::Sht4x) return NotEditable::No;
      return sht == ShtChip::Sht3x ? NotEditable::NoSht4x : NotEditable::NoSensor;
    case SettingId::Calibrate:
      return (mag == MagChip::None || mag == MagChip::Unknown) ? NotEditable::NoMagnetometer : NotEditable::No;
    default:
      return NotEditable::No;
  }
}

int16_t optionValue(SettingId id, int16_t v) {
  const SettingDef& d = settingDef(id);
  if (d.kind != SettingKind::Enum || !d.options) return v;
  return d.options[sanitize(id, v)];
}

int findSettingByKey(const char* nvsKey) {
  if (!nvsKey) return -1;
  for (int i = 0; i < (int)SettingId::Count; ++i)
    if (kDefs[i].nvsKey && strcmp(kDefs[i].nvsKey, nvsKey) == 0) return i;
  return -1;
}

}  // namespace hg
