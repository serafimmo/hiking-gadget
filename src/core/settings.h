#pragma once
// Numeric schema only; all user-visible text lives in ui/fmt + ui/strings*.
#include "types.h"
namespace hg {
enum class SettingId : uint8_t { TimeZone, SetTime, ScreenOff, Brightness, Animations, AutoOff, LightSleep,
  TempUnit, TempOffset, Heater, HeaterPower, Declination, Mount, NeedleSpeed, Calibrate, Diagnostics,
  PowerOff, ResetSettings, Version, Count };                                   // 19 rows, §8
enum class SettingKind : uint8_t { Enum, Number, Action, ActionHold, Info };
enum class Section : uint8_t { Clock, Display, Power, Sensor, Compass, System };
struct SettingDef { SettingId id; Section section; SettingKind kind; const char* nvsKey;
                    int16_t def, min, max, step; const int16_t* options; uint8_t nOptions; bool accel; };
struct Settings { int16_t v[(int)SettingId::Count]; int16_t get(SettingId) const; void set(SettingId, int16_t); };
enum class NotEditable : uint8_t { No, NoSht4x, NoSensor, NoMagnetometer };
const SettingDef& settingDef(SettingId id);
Settings defaultSettings();
bool     isValid(SettingId id, int16_t v);
int16_t  sanitize(SettingId id, int16_t v);                             // invalid -> default
int16_t  stepValue(SettingId id, int16_t cur, int dir, uint8_t step);   // Enum: option index +-1, clamped; Number: +-step*k, clamped
NotEditable editable(SettingId id, ShtChip sht, MagChip mag);           // e.g. Heater on SHT3x -> NoSht4x
// Enum settings store the option INDEX; optionValue() gives its meaning (seconds, minutes, enum value...).
// Number settings store the value itself (TempOffset in 0.1 C, Declination in degrees).
int16_t  optionValue(SettingId id, int16_t v);
int      findSettingByKey(const char* nvsKey);                          // -1 if no setting has that NVS key
}  // namespace hg
