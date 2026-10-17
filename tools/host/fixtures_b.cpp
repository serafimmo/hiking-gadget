// Fixtures and sweeps of screens B (SPEC §10.2): Compass, Settings, Calibration, Diagnostics.
#include <cmath>
#include <cstring>
#include <initializer_list>

#include "core/compass_math.h"
#include "core/tz.h"
#include "fixtures.h"
#include "ui/screens/screens.h"

using namespace hg;

namespace {
void heading(FixtureCtx& f, float deg) {
  f.vm.mag.headingDeg = deg; f.vm.mag.cardDeg = deg; f.vm.mag.shownDeg = shownHeading(deg, -1);
  f.vm.fmt.liveHeadingDeg = deg;
}
void target(FixtureCtx& f, int16_t bearing) {
  f.vm.target.set = true; f.vm.target.bearing = bearing;
  f.vm.target.steerDeg = wrap180((float)bearing - f.vm.mag.headingDeg);
}
void compass(FixtureCtx& f) { showPage(f, 3); }
void list(FixtureCtx& f, SettingId cursor, int dir = 1) {
  showPage(f, 4); pushModal(f, ScreenId::SettingsList);
  f.ui.listCursor = cursor;
  f.ui.listTop = listScroll(0, cursor, dir);
}
void picker(FixtureCtx& f, SettingId id, int16_t pending) {
  list(f, id); pushModal(f, ScreenId::Picker);
  f.ui.depth = 2; f.ui.modal[1] = ScreenId::Picker;
  f.ui.pending = pending;
}
void calib(FixtureCtx& f, uint32_t bins, float field, MagCalibrator::Hint h, MagCalibrator::Verdict v, float live) {
  compass(f); pushModal(f, ScreenId::Calibration);
  MagCalibrator::Progress& p = f.vm.cal.p;
  p.binsMask = bins; int n = 0; for (int i = 0; i < 24; ++i) n += (bins >> i) & 1;
  p.coveragePct = (uint8_t)(n * 100 / 24); p.fieldUt = field; p.hint = h; p.verdict = v; p.liveDeg = live;
}
void diag(FixtureCtx& f, uint8_t page) {
  list(f, SettingId::Diagnostics); pushModal(f, ScreenId::Diagnostics);
  f.ui.depth = 2; f.ui.modal[1] = ScreenId::Diagnostics;
  f.ui.diagPage = page;
}
}  // namespace

// ---------------------------------------------------------------- Compass
HG_FIXTURE(4, cmp_ok) { compass(f); heading(f, 45.2f); }
HG_FIXTURE(4, cmp_true) { compass(f); f.settings.set(SettingId::Declination, 12); heading(f, 57.2f); }
HG_FIXTURE(4, cmp_target) { compass(f); heading(f, 45.2f); target(f, 120); }
HG_FIXTURE(4, cmp_target_rel180) { compass(f); heading(f, 10.0f); target(f, 190); }
HG_FIXTURE(4, cmp_target_rel270) { compass(f); heading(f, 100.0f); target(f, 10); }
HG_FIXTURE(4, cmp_on_course) { compass(f); heading(f, 118.4f); target(f, 120); }
HG_FIXTURE(4, cmp_uncal) { compass(f); heading(f, 300.0f); f.vm.mag.calibrated = false; f.vm.fmt.calPct = -1; }
HG_FIXTURE(4, cmp_field) { compass(f); heading(f, 210.0f); f.vm.mag.fieldSuspect = true; }
HG_FIXTURE(4, cmp_saturated) { compass(f); heading(f, 210.0f); f.vm.mag.saturated = true; }
HG_FIXTURE(4, cmp_none) {
  compass(f); f.vm.mag.chip = MagChip::None; f.vm.mag.err = ErrCode::MagNone; f.vm.mag.valid = false; f.vm.bg.warn = true;
}
HG_FIXTURE(4, cmp_unknown) {
  compass(f); f.vm.mag.chip = MagChip::Unknown; f.vm.mag.addr = 0x2C; f.vm.mag.idByte = 0x12; f.vm.mag.err = ErrCode::MagUnknown;
  f.vm.mag.valid = false; f.vm.bg.warn = true;
}
HG_FIXTURE(4, cmp_chips) {
  compass(f); heading(f, 45.2f);
  f.vm.sw.st = Stopwatch::State::Running; f.vm.tm.st = Countdown::State::Running; f.vm.tm.remainingUs = 277 * kUsPerS;
  f.vm.tm.missed = true; refreshBg(f); f.vm.bg.warn = true;
}

// ---------------------------------------------------------------- Settings
HG_FIXTURE(5, set_cover) { showPage(f, 4); f.settings.set(SettingId::Declination, 12); }
HG_FIXTURE(5, set_list_top) { list(f, SettingId::TimeZone); }
HG_FIXTURE(5, set_list_sensor) { list(f, SettingId::Heater); f.settings.set(SettingId::TempOffset, 3); }
HG_FIXTURE(5, set_list_system_hold) {
  list(f, SettingId::PowerOff); f.ui.hold = HoldState{true, Key::Star, 0.7f};
}
HG_FIXTURE(5, set_list_sht3x) {
  list(f, SettingId::HeaterPower); f.vm.thermo.chip = ShtChip::Sht3x; f.vm.fmt.sht = ShtChip::Sht3x;
}
HG_FIXTURE(5, pick_tz) { picker(f, SettingId::TimeZone, 7); }
HG_FIXTURE(5, pick_tz_yekaterinburg) { picker(f, SettingId::TimeZone, 9); }
HG_FIXTURE(5, pick_scr) { picker(f, SettingId::ScreenOff, 2); }
HG_FIXTURE(5, pick_decl) { picker(f, SettingId::Declination, 12); }
HG_FIXTURE(5, pick_mount) { picker(f, SettingId::Mount, 7); }
HG_FIXTURE(5, pick_heater) { picker(f, SettingId::Heater, 3); }
HG_FIXTURE(5, pick_heater_na) {
  picker(f, SettingId::Heater, 0); f.vm.thermo.chip = ShtChip::Sht3x; f.vm.fmt.sht = ShtChip::Sht3x;
}
HG_FIXTURE(5, pick_toff) { picker(f, SettingId::TempOffset, -3); }
HG_FIXTURE(5, set_time_hh) {
  list(f, SettingId::SetTime); pushModal(f, ScreenId::SetTime); f.ui.depth = 2; f.ui.modal[1] = ScreenId::SetTime;
  f.ui.timeEdit = f.vm.clock.local; f.ui.timeEdit.second = 0; f.ui.timeField = 0;
}
HG_FIXTURE(5, set_time_year) {
  list(f, SettingId::SetTime); pushModal(f, ScreenId::SetTime); f.ui.depth = 2; f.ui.modal[1] = ScreenId::SetTime;
  f.ui.timeEdit = f.vm.clock.local; f.ui.timeField = 4;
}

// ---------------------------------------------------------------- Calibration
HG_FIXTURE(6, cal_start) { calib(f, 0, 0, MagCalibrator::Hint::TurnMore, MagCalibrator::Verdict::Collecting, 0); }
HG_FIXTURE(6, cal_progress) { calib(f, 0x0003FF, 16.4f, MagCalibrator::Hint::TurnMore, MagCalibrator::Verdict::Collecting, 160); }
HG_FIXTURE(6, cal_keep_level) { calib(f, 0x00FFFF, 16.4f, MagCalibrator::Hint::KeepLevel, MagCalibrator::Verdict::Collecting, 250); }
HG_FIXTURE(6, cal_metal_near) { calib(f, 0x00FF0F, 31.0f, MagCalibrator::Hint::MetalNear, MagCalibrator::Verdict::Collecting, 100); }
HG_FIXTURE(6, cal_ready) { calib(f, 0xFFFFFF, 16.6f, MagCalibrator::Hint::Ready, MagCalibrator::Verdict::Ready, 30); }
HG_FIXTURE(6, cal_good) {
  calib(f, 0xFFFFFF, 16.6f, MagCalibrator::Hint::Ready, MagCalibrator::Verdict::Ready, 30);
  f.vm.cal.haveResult = true;
  MagCalibrator::Result& r = f.vm.cal.r;
  r.grade = MagCalibrator::Grade::Good; r.cal.qualityPct = 94; r.cal.offX = -312; r.cal.offY = 87;
  r.cal.scaleX = 1.03f; r.cal.scaleY = 0.97f; r.fieldUt = 16.4f; r.coveragePct = 100; r.cal.valid = true;
}
HG_FIXTURE(6, cal_failed) {
  calib(f, 0x0001FF, 16.6f, MagCalibrator::Hint::TurnMore, MagCalibrator::Verdict::Failed, 30);
}

// ---------------------------------------------------------------- Diagnostics
HG_FIXTURE(7, diag_sensors) { diag(f, 0); f.vm.mag.drdyPerS = 50; f.vm.sys.heatValid = true; f.vm.sys.heatPreC10 = 234; f.vm.sys.heatHotC10 = 541; }
HG_FIXTURE(7, diag_i2c) { diag(f, 1); }
HG_FIXTURE(7, diag_buttons) { diag(f, 2); f.vm.sys.keyPresses[0] = 3; f.vm.sys.keyPresses[2] = 12; f.vm.sys.keyDownMask = 4; }
HG_FIXTURE(7, diag_mag) {
  diag(f, 3);
  for (int i = 0; i < 3; ++i) { f.vm.mag.mn[i] = (int16_t)(f.vm.mag.raw[i] - 1500); f.vm.mag.mx[i] = (int16_t)(f.vm.mag.raw[i] + 1400); }
}
HG_FIXTURE(7, diag_system) { diag(f, 4); f.vm.sys.coreVer = "3.3.12 IDF 5.5.5"; f.vm.sys.wakeCause = WakeCause::Ext1; f.vm.sys.wakeKey = 1; f.vm.sys.wakeSampleUs = 212000; }

// ---------------------------------------------------------------- Sweeps
HG_SWEEP(settings_every_value) {
  for (int i = 0; i < (int)SettingId::Count; ++i) {
    const SettingId id = (SettingId)i;
    const SettingDef& d = settingDef(id);
    int16_t vals[40]; int n = 0;
    if (d.kind == SettingKind::Enum) for (int v = d.min; v <= d.max && n < 40; ++v) vals[n++] = (int16_t)v;
    else if (d.kind == SettingKind::Number) { vals[n++] = d.min; vals[n++] = d.max; vals[n++] = d.def; vals[n++] = (int16_t)(d.min + d.step); }
    else vals[n++] = 0;
    for (int k = 0; k < n; ++k)
      for (int sht = 0; sht < 2; ++sht) {
        FixtureCtx f = baseWorld();
        if (sht) { f.vm.fmt.sht = ShtChip::Sht3x; f.vm.thermo.chip = ShtChip::Sht3x; }
        f.settings.set(id, vals[k]);
        f.vm.fmt.liveTempC = -39.9f;
        f.vm.fmt.liveHeadingDeg = 359.4f;
        // list: cursor on the row, and on another row with this row visible
        list(f, id);
        out.emit("list_on", f);
        f.ui.listCursor = id == SettingId::Version ? SettingId::ResetSettings : SettingId::Version;
        f.ui.listTop = listScroll(listRowOf(id) > 0 ? (uint8_t)(listRowOf(id) - 1) : 0, f.ui.listCursor, 0);
        out.emit("list_off", f);
        // cover
        FixtureCtx c = f; showPage(c, 4); out.emit("cover", c);
        // picker
        if (d.kind == SettingKind::Enum || d.kind == SettingKind::Number) {
          FixtureCtx p = f; picker(p, id, vals[k]); out.emit("picker", p);
        }
      }
  }
}
HG_SWEEP(tz_every_zone_picker) {
  for (int z = 0; z < kTzCount; ++z)
    for (int64_t utc : {(int64_t)1791736927, (int64_t)1767225600}) {
      FixtureCtx f = baseWorld(); setClock(f, utc, kTzTable[z].id); picker(f, SettingId::TimeZone, (int16_t)z);
      out.emit("tz_picker", f);
      FixtureCtx l = baseWorld(); setClock(l, utc, kTzTable[z].id); list(l, SettingId::TimeZone);
      out.emit("tz_list", l);
    }
}
HG_SWEEP(compass_bearings_and_headings) {
  for (int rel = 0; rel < 360; rel += 15)
    for (float h : {0.0f, 45.2f, 133.7f, 271.9f}) {
      FixtureCtx f = baseWorld(); compass(f); heading(f, h); target(f, (int16_t)((int)lroundf(h) + rel) % 360);
      out.emit("cmp_target", f);
    }
  for (int h = 0; h < 360; ++h) {
    FixtureCtx f = baseWorld(); compass(f); heading(f, (float)h);
    if (h % 3 == 1) f.settings.set(SettingId::Declination, -30);
    out.emit("cmp_heading", f);
  }
}
HG_SWEEP(calibration_all_bins_and_hints) {
  for (int k = 0; k <= 24; ++k)
    for (int h = 0; h < 6; ++h) {
      FixtureCtx f = baseWorld();
      calib(f, k == 24 ? 0xFFFFFFu : ((1u << k) - 1u), 59.6f, (MagCalibrator::Hint)h,
            h == 5 ? MagCalibrator::Verdict::Ready : MagCalibrator::Verdict::Collecting, (float)(k * 15));
      out.emit("cal", f);
    }
}
