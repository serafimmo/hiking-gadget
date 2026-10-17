// app/persist over FakeKv (SPEC §8.2).
#include "app/persist.h"
#include "core/tz.h"
#include "fakes/fake_ports.h"
#include "tiny_test.h"

using namespace hg;

TEST(persist_missing_keys_are_defaults) {
  FakeKv kv;
  Persist p(&kv);
  Settings s{};
  const Persist::LoadResult r = p.loadSettings(s);
  CHECK(!r.anyInvalid);
  for (int i = 0; i < (int)SettingId::Count; ++i) CHECK_EQ(s.get((SettingId)i), settingDef((SettingId)i).def);
  CHECK_EQ(p.loadPreset(), 2);
}

TEST(persist_round_trip_and_tz_by_id) {
  FakeKv kv;
  Persist p(&kv);
  CHECK(p.saveSetting(SettingId::TimeZone, (int16_t)findTz("CET")));
  CHECK(p.saveSetting(SettingId::TempOffset, -37));
  CHECK(p.saveSetting(SettingId::ScreenOff, 5));
  Settings s{};
  p.loadSettings(s);
  CHECK_EQ(s.get(SettingId::TimeZone), findTz("CET"));
  CHECK_EQ(s.get(SettingId::TempOffset), -37);
  CHECK_EQ(s.get(SettingId::ScreenOff), 5);
  char z[4];
  CHECK(kv.get("tz", z, 4));
  CHECK(z[0] == 'C' && z[1] == 'E' && z[2] == 'T');                 // stable id, not the index
}

TEST(persist_invalid_value_sanitized_and_reported) {
  FakeKv kv;
  const uint8_t bad = 9;
  kv.put("scr", &bad, 1);
  const int8_t badToff = 90;
  kv.put("toff", &badToff, 1);
  Persist p(&kv);
  Settings s{};
  const Persist::LoadResult r = p.loadSettings(s);
  CHECK(r.anyInvalid);
  CHECK_EQ(r.firstInvalid, SettingId::ScreenOff);
  CHECK_EQ(r.firstOld, 9);
  CHECK_EQ(r.invalidCount, 2);
  CHECK_EQ(s.get(SettingId::ScreenOff), settingDef(SettingId::ScreenOff).def);
  CHECK_EQ(s.get(SettingId::TempOffset), 0);
}

TEST(persist_cal_and_clk_blobs_with_crc) {
  FakeKv kv;
  Persist p(&kv);
  MagCal c{};
  c.chip = 1; c.offX = -312; c.offY = 87; c.scaleX = 1.03f; c.scaleY = 0.97f; c.rCalCounts = 498; c.lsbPerUt = 30; c.qualityPct = 94; c.valid = true;
  CHECK(p.saveCal(c, 1791736927));
  MagCal r{};
  int64_t t = 0;
  CHECK(p.loadCal(r, &t));
  CHECK_EQ(t, (int64_t)1791736927);
  CHECK_NEAR(r.offX, -312.0f, 1e-3f); CHECK_NEAR(r.scaleY, 0.97f, 1e-6f); CHECK_EQ(r.qualityPct, 94); CHECK(r.valid);
  FakeKv::Entry* e = kv.find("cal");
  e->data[10] ^= 0x40;                                               // damage
  CHECK(!p.loadCal(r, nullptr));
  const ClockMeta m{ClockQuality::Synced, ClockSource::Serial, 1791736000, -1840};
  CHECK(p.saveClk(1791736927, m));
  int64_t u = 0;
  ClockMeta mm{};
  CHECK(p.loadClk(u, mm));
  CHECK_EQ(u, (int64_t)1791736927); CHECK_EQ(mm.q, ClockQuality::Synced); CHECK_EQ(mm.lastSyncDeltaMs, -1840);
  kv.find("clk")->data[5] ^= 1;
  CHECK(!p.loadClk(u, mm));
}

TEST(persist_reset_keeps_cal_clk_bld) {
  FakeKv kv;
  Persist p(&kv);
  p.saveSetting(SettingId::Heater, 3);
  p.savePreset(4);
  MagCal c{}; c.chip = 1; c.scaleX = c.scaleY = 1; c.rCalCounts = 400; c.valid = true;
  p.saveCal(c, 1);
  p.saveClk(1791736927, ClockMeta{ClockQuality::Synced, ClockSource::Serial, 0, 0});
  p.saveBuildId(0x1880C947);
  CHECK(p.resetSettings());
  Settings s{};
  p.loadSettings(s);
  CHECK_EQ(s.get(SettingId::Heater), 0);
  CHECK_EQ(p.loadPreset(), 2);
  MagCal r{};
  CHECK(p.loadCal(r, nullptr));
  int64_t u; ClockMeta m;
  CHECK(p.loadClk(u, m));
  CHECK_EQ(p.loadBuildId(), 0x1880C947u);
}

TEST(persist_put_failure_counted) {
  FakeKv kv;
  kv.failPuts = true;
  Persist p(&kv);
  CHECK(!p.saveSetting(SettingId::Brightness, 2));
  CHECK_EQ(p.putFailures(), 1u);
  CHECK_STR(p.lastFailedKey(), "bri");
  FakeKv gone;
  gone.present = false;
  Persist q(&gone);
  CHECK(!q.ok());
  Settings s{};
  q.loadSettings(s);                                                 // RAM-only defaults
  CHECK_EQ(s.get(SettingId::ScreenOff), 2);
}
