// core/settings: schema (§8.1), validation, stepping, editability.
#include <cstring>

#include "core/settings.h"
#include "core/tz.h"
#include "tiny_test.h"

using namespace hg;

TEST(settings_schema_and_defaults) {
  const Settings d = defaultSettings();
  int keys = 0;
  for (int i = 0; i < (int)SettingId::Count; ++i) {
    const SettingId id = (SettingId)i;
    const SettingDef& def = settingDef(id);
    CHECK_EQ(def.id, id);
    CHECK(isValid(id, d.get(id)));
    if (def.nvsKey) {
      ++keys;
      CHECK(strlen(def.nvsKey) <= 5);                       // NVS keys <= 15; CmdLine.key holds 7
      CHECK_EQ(findSettingByKey(def.nvsKey), i);
    }
    if (def.kind == SettingKind::Enum) CHECK_EQ(def.max + 1, (int)(def.options ? def.nOptions : kTzCount));
  }
  CHECK_EQ(keys, 13);                                       // "erases the 13 setting keys"
  CHECK_EQ(findSettingByKey("nope"), -1);
  CHECK_EQ(optionValue(SettingId::ScreenOff, d.get(SettingId::ScreenOff)), 30);   // 30 s
  CHECK_EQ(optionValue(SettingId::AutoOff, d.get(SettingId::AutoOff)), 30);       // 30 min
  CHECK_EQ(optionValue(SettingId::Animations, d.get(SettingId::Animations)), 1);  // On
  CHECK_EQ(optionValue(SettingId::LightSleep, d.get(SettingId::LightSleep)), 1);  // On
  CHECK_EQ(d.get(SettingId::TimeZone), (int)kTzDefault);
  CHECK_EQ(d.get(SettingId::Brightness), 1);                // Mid
  CHECK_EQ(d.get(SettingId::HeaterPower), 1);               // Mid 110 mW
  CHECK_EQ(d.get(SettingId::NeedleSpeed), 1);               // Normal
}

TEST(settings_edges_and_sanitize) {
  CHECK(isValid(SettingId::TempOffset, -50)); CHECK(isValid(SettingId::TempOffset, 50));
  CHECK(!isValid(SettingId::TempOffset, 51)); CHECK(!isValid(SettingId::TempOffset, -51));
  CHECK(isValid(SettingId::Declination, -30)); CHECK(!isValid(SettingId::Declination, 31));
  CHECK(!isValid(SettingId::ScreenOff, 7)); CHECK(!isValid(SettingId::ScreenOff, -1));
  CHECK(!isValid(SettingId::TimeZone, 18));
  CHECK(!isValid(SettingId::Version, 3)); CHECK(isValid(SettingId::Version, 0));
  CHECK(!isValid(SettingId::Count, 0));
  CHECK_EQ(sanitize(SettingId::ScreenOff, 99), 2);
  CHECK_EQ(sanitize(SettingId::Mount, 7), 7);
}

TEST(settings_step_clamp_and_accel) {
  CHECK_EQ(stepValue(SettingId::ScreenOff, 0, -1, 1), 0);   // clamped at the ends
  CHECK_EQ(stepValue(SettingId::ScreenOff, 6, +1, 1), 6);
  CHECK_EQ(stepValue(SettingId::ScreenOff, 2, +1, 5), 3);   // Enum ignores accel
  CHECK_EQ(stepValue(SettingId::TempOffset, 0, +1, 1), 1);
  CHECK_EQ(stepValue(SettingId::TempOffset, 0, +1, 5), 5);  // accel x5
  CHECK_EQ(stepValue(SettingId::TempOffset, 48, +1, 5), 50);
  CHECK_EQ(stepValue(SettingId::TempOffset, -48, -1, 5), -50);
  CHECK_EQ(stepValue(SettingId::Declination, 30, +1, 1), 30);
  CHECK_EQ(stepValue(SettingId::TimeZone, 17, +1, 1), 17);
  CHECK_EQ(stepValue(SettingId::SetTime, 0, +1, 1), 0);     // actions do not step
  CHECK_EQ(stepValue(SettingId::TempOffset, 99, -1, 1), -1);   // an invalid current value is sanitized first
}

TEST(settings_editable) {
  CHECK_EQ(editable(SettingId::Heater, ShtChip::Sht4x, MagChip::None), NotEditable::No);
  CHECK_EQ(editable(SettingId::Heater, ShtChip::Sht3x, MagChip::None), NotEditable::NoSht4x);
  CHECK_EQ(editable(SettingId::HeaterPower, ShtChip::Sht3x, MagChip::None), NotEditable::NoSht4x);
  CHECK_EQ(editable(SettingId::Heater, ShtChip::None, MagChip::None), NotEditable::NoSensor);
  CHECK_EQ(editable(SettingId::Calibrate, ShtChip::None, MagChip::None), NotEditable::NoMagnetometer);
  CHECK_EQ(editable(SettingId::Calibrate, ShtChip::None, MagChip::Qmc5883l), NotEditable::No);
  CHECK_EQ(editable(SettingId::TempOffset, ShtChip::None, MagChip::None), NotEditable::No);
}

TEST(settings_tz_id_round_trip) {
  for (int i = 0; i < kTzCount; ++i) {
    CHECK(isValid(SettingId::TimeZone, (int16_t)i));
    CHECK_EQ(findTz(kTzTable[i].id), i);
  }
}
