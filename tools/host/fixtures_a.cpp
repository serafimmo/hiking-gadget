// Fixtures and sweeps of screens A (SPEC §10.2): Home, Stopwatch, Timer, Alert, cards, power off, splash.
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "core/tz.h"
#include "fixtures.h"
#include "ui/strings.h"

using namespace hg;

namespace {
constexpr int64_t S = kUsPerS;
int64_t utcFor(int y, int mo, int d, int h, int mi, int s, const char* tz) {
  Civil c{};
  c.year = (int16_t)y; c.month = (uint8_t)mo; c.day = (uint8_t)d; c.hour = (uint8_t)h; c.minute = (uint8_t)mi; c.second = (uint8_t)s;
  return localToUtc(c, kTzTable[findTz(tz)]);
}
void timerRunning(FixtureCtx& f, uint8_t preset, int64_t totalUs, int64_t remainingUs) {
  TimerView& t = f.vm.tm;
  t.st = Countdown::State::Running;
  t.preset = preset;
  t.totalUs = totalUs;
  t.remainingUs = remainingUs;
  t.endsKnown = true;
  t.endsLocal = utcToLocal(f.vm.clock.utcS + (remainingUs + S - 1) / S, kTzTable[f.settings.get(SettingId::TimeZone)]);
  refreshBg(f);
}
void swRunning(FixtureCtx& f, int64_t elapsed, uint16_t lapNo, int64_t cur, int64_t last, int64_t delta) {
  StopwatchView& s = f.vm.sw;
  s.st = Stopwatch::State::Running;
  s.elapsedUs = elapsed; s.curLapUs = cur; s.lapNo = lapNo; s.laps = (uint8_t)(lapNo > 99 ? 99 : lapNo);
  s.lastLapUs = last; s.lastDeltaUs = delta;
  refreshBg(f);
}
void allChips(FixtureCtx& f) {
  swRunning(f, 754560000, 4, 62310000, 181200000, -420000);
  timerRunning(f, 2, 300 * S, 277 * S);
  f.vm.tm.missed = true;
  refreshBg(f);
  f.vm.bg.warn = true;
}
void timerDone(FixtureCtx& f, int64_t totalUs, int64_t overtimeUs) {
  TimerView& t = f.vm.tm;
  t.st = Countdown::State::Done;
  t.totalUs = totalUs; t.remainingUs = 0; t.overtimeUs = overtimeUs;
  t.endsKnown = true;
  t.endsLocal = utcToLocal(f.vm.clock.utcS - overtimeUs / S, kTzTable[f.settings.get(SettingId::TimeZone)]);
  refreshBg(f);
}
}  // namespace

// ---------------------------------------------------------------- Home
HG_FIXTURE(1, home_synced) { (void)f; }
HG_FIXTURE(1, home_bg_chips) {
  swRunning(f, 754560000, 4, 62310000, 181200000, -420000);
  timerRunning(f, 2, 300 * S, 277 * S);
}
HG_FIXTURE(1, home_approx) {
  f.vm.clock.q = ClockQuality::Approx; f.vm.clock.src = ClockSource::Build;
  refreshBg(f);
}
HG_FIXTURE(1, home_all_chips_approx) {   // widest date MON 28 MAR, zone OMST, Approx and all four chips
  setClock(f, utcFor(2033, 3, 28, 14, 5, 27, "OMS"), "OMS");
  f.vm.clock.q = ClockQuality::Approx; f.vm.clock.src = ClockSource::Restored;
  allChips(f);
}
HG_FIXTURE(1, home_notime) {
  f.vm.clock.valid = false; f.vm.clock.q = ClockQuality::None;
  refreshBg(f);
}
HG_FIXTURE(1, home_nosensor) {
  f.vm.thermo.chip = ShtChip::None; f.vm.thermo.h = Health::Absent; f.vm.thermo.valid = false;
  f.vm.thermo.err = ErrCode::ShtNone;
  f.vm.bg.warn = true;
}
HG_FIXTURE(1, home_unknown_sht) {
  f.vm.thermo.chip = ShtChip::Unknown; f.vm.thermo.h = Health::Fault; f.vm.thermo.valid = false;
  f.vm.thermo.err = ErrCode::ShtUnknown;
  f.vm.bg.warn = true;
}
HG_FIXTURE(1, home_cold_heat) {
  setTemp(f, -12.3f, 85.0f);
  f.vm.thermo.heaterUi = 2;
}
HG_FIXTURE(1, home_stale) { f.vm.thermo.stale = true; }
HG_FIXTURE(1, home_details) { f.ui.details = true; }
HG_FIXTURE(1, home_hold_off) { f.ui.hold = HoldState{true, Key::Star, 0.6f}; }

// ---------------------------------------------------------------- Stopwatch
HG_FIXTURE(2, sw_idle) { showPage(f, 1); }
HG_FIXTURE(2, sw_running) { showPage(f, 1); swRunning(f, 754560000, 4, 62310000, 181200000, -420000); }
HG_FIXTURE(2, sw_paused) {
  showPage(f, 1); swRunning(f, 754560000, 4, 62310000, 181200000, 1020000);
  f.vm.sw.st = Stopwatch::State::Paused; refreshBg(f);
}
HG_FIXTURE(2, sw_paused_all_chips) {   // PAUSED + warning + bell + timer: the timer chip is dropped (D30)
  showPage(f, 1); allChips(f);
  f.vm.sw.st = Stopwatch::State::Paused;
}
HG_FIXTURE(2, sw_reset_hold) {
  showPage(f, 1); swRunning(f, 754560000, 4, 62310000, 181200000, -420000);
  f.vm.sw.st = Stopwatch::State::Paused; refreshBg(f);
  f.ui.hold = HoldState{true, Key::Hash, 0.5f};
}
HG_FIXTURE(2, sw_hour) { showPage(f, 1); swRunning(f, 3733400000LL, 12, 302310000, 297000000, 5310000); }
HG_FIXTURE(2, sw_10h) { showPage(f, 1); swRunning(f, 36000LL * S, 40, 1200 * S, 900 * S, -62 * S); }
HG_FIXTURE(2, sw_max) {
  showPage(f, 1); swRunning(f, Stopwatch::kMaxElapsedUs, 3, 1234 * S, 3600 * S, 3599 * S);
  f.vm.sw.st = Stopwatch::State::Paused; refreshBg(f);
}
HG_FIXTURE(2, sw_lap_999) {
  showPage(f, 1); swRunning(f, 359990LL * S, 999, 0, 359999LL * S, 359000LL * S);
  f.vm.sw.st = Stopwatch::State::Paused; refreshBg(f);
}

// ---------------------------------------------------------------- Timer
HG_FIXTURE(3, tm_select) {
  showPage(f, 2);
  f.vm.tm.endsKnown = true;
  f.vm.tm.endsLocal = utcToLocal(f.vm.clock.utcS + 300, kTzTable[kTzDefault]);
}
HG_FIXTURE(3, tm_running) { showPage(f, 2); timerRunning(f, 2, 300 * S, 277 * S); }
HG_FIXTURE(3, tm_running_60) { showPage(f, 2); timerRunning(f, 5, 3600 * S, 3599 * S); }
HG_FIXTURE(3, tm_paused_on) {
  showPage(f, 2); timerRunning(f, 2, 300 * S, 277 * S);
  f.vm.tm.st = Countdown::State::Paused; refreshBg(f);
  f.now = MonoUs{1000LL * S + 200000};
}
HG_FIXTURE(3, tm_paused_off) {
  showPage(f, 2); timerRunning(f, 2, 300 * S, 277 * S);
  f.vm.tm.st = Countdown::State::Paused; refreshBg(f);
  f.now = MonoUs{1000LL * S + 800000};
}
HG_FIXTURE(3, tm_hour) { showPage(f, 2); timerRunning(f, 5, 90 * 60 * S, 4320 * S); }
HG_FIXTURE(3, tm_done) { showPage(f, 2); timerDone(f, 300 * S, 133 * S); }
HG_FIXTURE(3, tm_done_overtime_1h) { showPage(f, 2); timerDone(f, 300 * S, 3733 * S); }
HG_FIXTURE(3, alert_a) {
  showPage(f, 3); timerDone(f, 300 * S, 12 * S);
  f.vm.tm.alertActive = true; f.vm.tm.alertPhase = 0;
}
HG_FIXTURE(3, alert_b) {
  showPage(f, 0); timerDone(f, 300 * S, 95 * S);
  f.vm.tm.alertActive = true; f.vm.tm.alertPhase = 1;
}

// ---------------------------------------------------------------- Cards and system
HG_FIXTURE(8, card_check_hw) {
  f.vm.thermo.chip = ShtChip::None; f.vm.thermo.err = ErrCode::ShtNone; f.vm.thermo.addr = 0x44;
  f.ui.card = ErrCode::ShtNone;
}
HG_FIXTURE(8, card_check_hw_multi) {   // 4 failures -> the last slot says "+N more, see Serial"
  f.vm.thermo.err = ErrCode::ShtNone; f.vm.thermo.addr = 0x44;
  f.vm.mag.err = ErrCode::MagNone;
  f.vm.sys.busErr = ErrCode::I2cStuck;
  f.vm.sys.nvsErr = ErrCode::NvsFail;
  f.ui.card = ErrCode::ShtNone;
}
HG_FIXTURE(8, card_i2c_stuck) { f.ui.card = ErrCode::I2cStuck; }
HG_FIXTURE(8, card_key_stuck) { f.ui.card = ErrCode::KeyStuck; f.ui.cardArg = (uint8_t)(2 | (1 << 4)); }
HG_FIXTURE(8, card_key_stuck_runtime) { f.ui.card = ErrCode::KeyStuck; f.ui.cardArg = 2; }
HG_FIXTURE(8, card_key_stuck_phantom) { f.ui.card = ErrCode::KeyStuck; f.ui.cardArg = (uint8_t)(1 | (2 << 4)); }
HG_FIXTURE(8, card_state_lost) { f.ui.card = ErrCode::RtcLost; f.ui.cardArg = 0; }
HG_FIXTURE(8, card_timer_lost) { f.ui.card = ErrCode::RtcLost; f.ui.cardArg = 1; }
HG_FIXTURE(8, card_crash_loop) { f.ui.card = ErrCode::RtcLost; f.ui.cardArg = 2; }
HG_FIXTURE(8, card_cfg_reset) { f.ui.card = ErrCode::CfgReset; f.ui.cardArg = (uint8_t)SettingId::ScreenOff; }
HG_FIXTURE(8, card_sht3x_info) { f.ui.card = ErrCode::Sht3xInfo; }
HG_FIXTURE(8, power_off) { f.overlay = ScreenId::PowerOff; }
HG_FIXTURE(8, power_off_timer) { timerRunning(f, 3, 600 * S, 500 * S); f.overlay = ScreenId::PowerOff; }
HG_FIXTURE(8, splash) { f.overlay = ScreenId::Splash; }

// ---------------------------------------------------------------- Sweeps (audited, not rendered)
HG_SWEEP(home_every_zone) {
  for (int z = 0; z < kTzCount; ++z)
    for (int64_t utc : {(int64_t)1791736927 /*Oct: EU DST*/, (int64_t)1767225600 /*Jan 1*/, (int64_t)1785000000 /*Jul*/}) {
      FixtureCtx f = baseWorld();
      setClock(f, utc, kTzTable[z].id);
      out.emit(kTzTable[z].id, f);
    }
}
HG_SWEEP(home_top_line_combos) {
  const struct { int y, mo, d; } dates[2] = {{2033, 3, 28} /*MON 28 MAR*/, {2026, 1, 1} /*THU 1 JAN*/};
  const char* zones[2] = {"OMS", "UTC"};
  for (int m = 0; m < 32; ++m)
    for (const auto& dt : dates)
      for (const char* z : zones) {
        FixtureCtx f = baseWorld();
        setClock(f, utcFor(dt.y, dt.mo, dt.d, 12, 0, 0, z), z);
        if (m & 16) { f.vm.clock.q = ClockQuality::Approx; f.vm.clock.src = ClockSource::Build; }
        if (m & 2) swRunning(f, 5 * S, 0, 5 * S, 0, 0);
        if (m & 4) timerRunning(f, 2, 300 * S, 59 * S);
        if (m & 8) f.vm.tm.missed = true;
        refreshBg(f);
        f.vm.bg.warn = (m & 1) != 0;
        out.emit("home_top", f);
      }
}
HG_SWEEP(status_line_chips_and_tags) {
  for (uint8_t page : {(uint8_t)1, (uint8_t)2, (uint8_t)4})
    for (int m = 0; m < 16; ++m)
      for (int tagState = 0; tagState < 3; ++tagState) {
        FixtureCtx f = baseWorld();
        showPage(f, page);
        if (m & 2) swRunning(f, 5 * S, 0, 5 * S, 0, 0);
        if (m & 4) timerRunning(f, 5, 3600 * S, 35999 * S / 10);
        if (m & 8) f.vm.tm.missed = true;
        refreshBg(f);
        f.vm.bg.warn = m & 1;
        f.vm.bg.tmrRemainS = 35999;
        if (page == 1 && tagState) { f.vm.sw.st = Stopwatch::State::Paused; f.vm.sw.elapsedUs = 3 * S; }
        if (page == 2 && tagState == 1) { f.vm.tm.st = Countdown::State::Paused; f.vm.tm.remainingUs = 30 * S; f.vm.tm.totalUs = 60 * S; }
        if (page == 2 && tagState == 2) { f.vm.tm.st = Countdown::State::Done; f.vm.tm.overtimeUs = 30 * S; f.vm.tm.totalUs = 60 * S; }
        out.emit("status_line", f);
      }
}
HG_SWEEP(timer_states) {
  const int64_t totals[] = {60 * S, 99 * 60 * S, 100 * 60 * S, 600 * 60 * S};
  const int64_t remains[] = {1 * S, 3599 * S, 36000 * S};
  const int64_t overs[] = {0, 3599 * S, 3600 * S, 359999 * S};
  for (uint8_t p = 0; p < Countdown::kPresetCount; ++p) {
    FixtureCtx f = baseWorld(); showPage(f, 2); f.vm.tm.preset = p;
    f.vm.tm.totalUs = f.vm.tm.remainingUs = Countdown::kPresetMin[p] * 60 * S;
    out.emit("tm_select", f);
  }
  for (int64_t tot : totals)
    for (int64_t rem : remains) {
      if (rem > tot) continue;
      for (int st = 0; st < 2; ++st) {
        FixtureCtx f = baseWorld(); showPage(f, 2); timerRunning(f, 0, tot, rem);
        if (st) { f.vm.tm.st = Countdown::State::Paused; refreshBg(f); }
        out.emit("tm_run", f);
      }
      for (int64_t ov : overs) { FixtureCtx f = baseWorld(); showPage(f, 2); timerDone(f, tot, ov); out.emit("tm_done", f); }
    }
  for (int64_t ov : overs) {
    FixtureCtx f = baseWorld(); timerDone(f, 600 * 60 * S, ov); f.vm.tm.alertActive = true; out.emit("alert", f);
  }
}
HG_SWEEP(stopwatch_states) {
  const int64_t el[] = {0, 3599990000LL, 35999900000LL, 359990LL * S, Stopwatch::kMaxElapsedUs};
  const uint16_t laps[] = {1, 9, 99, 100, 999};
  const int64_t deltas[] = {0, 59990000, 60 * S, 3599 * S, 3600 * S, 359999LL * S};
  for (int64_t e : el)
    for (uint16_t l : laps)
      for (int64_t d : deltas)
        for (int sign = -1; sign <= 1; sign += 2) {
          FixtureCtx f = baseWorld(); showPage(f, 1);
          swRunning(f, e, l, e > 359999LL * S ? 359999LL * S : e, 359999LL * S, sign * d);
          out.emit("sw", f);
        }
}
HG_SWEEP(cards_longest_and_error_texts) {
  for (int e = 0; e < (int)ErrCode::Count; ++e) {
    FixtureCtx f = baseWorld();
    f.vm.thermo.err = (ErrCode)e; f.vm.thermo.addr = 0x45;
    f.vm.mag.err = (ErrCode)e; f.vm.mag.addr = 0x2C;
    f.vm.sys.busErr = (ErrCode)e;
    f.ui.card = (ErrCode)e == ErrCode::None ? ErrCode::ShtNone : (ErrCode)e;
    f.ui.cardArg = 3 | (2 << 4);
    out.emit("card", f);
  }
  for (int id = 0; id < (int)SettingId::Count; ++id) {
    FixtureCtx f = baseWorld(); f.ui.card = ErrCode::CfgReset; f.ui.cardArg = (uint8_t)id; out.emit("cfg_reset", f);
  }
  for (uint8_t k = 0; k < 4; ++k)
    for (uint8_t v = 0; v < 3; ++v) {
      FixtureCtx f = baseWorld(); f.ui.card = ErrCode::KeyStuck; f.ui.cardArg = (uint8_t)(k | (v << 4)); out.emit("key_stuck", f);
    }
}
