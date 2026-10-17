// HOME (SPEC §3.8.1, bindings §2.4)
#include <stdio.h>
#include <string.h>

#include "../../config/timing.h"
#include "../canvas.h"
#include "../fmt.h"
#include "../strings.h"
#include "../theme.h"
#include "../widgets.h"
#include "screens.h"

namespace hg {
namespace {
void climateRow(Canvas& c, const ViewModel& vm) {
  const ThermoView& t = vm.thermo;
  if (t.chip == ShtChip::None || t.chip == ShtChip::Unknown) {
    c.icon(0, 49, IconId::SWarn);
    c.font(Font::Body);
    if (t.chip == ShtChip::Unknown) {
      char a[8], line[32];
      snprintf(a, sizeof a, "0x%02X", t.addr);
      snprintf(line, sizeof line, str(S_HOME_UNKNOWN), a);
      c.text(8, 48, line);
      c.text(8, 57, str(S_HOME_NOT_SHT));
    } else {
      c.text(8, 48, str(S_HOME_NO_SENSOR));
      c.text(8, 57, str(S_HOME_CHECK_WIRES));
    }
    return;
  }
  const bool f = useFahrenheit(vm);
  const bool show = t.valid && !t.stale;
  char tt[12], rh[8];
  if (show) {
    fmtTemp(tt, sizeof tt, t.tC, f);
    snprintf(rh, sizeof rh, "%d", (int)(t.rh + 0.5f));
  } else {
    strcpy(tt, "--.-");
    strcpy(rh, "--");
  }
  c.font(Font::Mid);
  const int16_t w = c.num(0, 48, tt);
  c.degree((int16_t)(w + 1), 48, false);
  c.font(Font::Body);
  c.text((int16_t)(w + 5), 49, f ? "F" : "C");
  if (t.heaterUi) c.icon((int16_t)(w + 5), 58, IconId::SFlame);
  const int16_t rx = w + 15 > 50 ? (int16_t)(w + 15) : 50;
  c.font(Font::Mid);
  const int16_t rw = c.num(rx, 48, rh);
  c.font(Font::Body);
  const int16_t pw = c.text((int16_t)(rx + rw + 2), 49, str(S_HOME_RH));
  if (show && rx + rw + 2 + pw - 1 <= 93) {   // dew point only when the RH group ends <= x 93
    char d[12];
    fmtTemp(d, sizeof d, t.dewC, f);
    c.text(kContentX1, 48, str(S_HOME_DEW), Align::Right);
    c.num(113, 57, d, Align::Right);
    c.degree(114, 57, false);
  }
}

void detailRow(Canvas& c, int i, uint16_t key, const char* value) {
  const int16_t ink = (int16_t)(10 + 9 * i);
  c.font(Font::Body);
  c.text(0, ink, str(key));
  c.text(kContentX1, ink, value, Align::Right);
}

void details(Canvas& c, const ViewModel& vm) {
  char clk[12] = "";
  if (vm.clock.valid) fmtClockSec(clk, sizeof clk, vm.clock.local.hour, vm.clock.local.minute, vm.clock.local.second);
  widgets::statusLine(c, str(S_DET_TITLE), nullptr, clk, nullptr);
  char v[40], a[16];
  // Zone
  char off[12];
  fmtUtcOffset(off, sizeof off, vm.clock.offsetMin);
  snprintf(v, sizeof v, "%s  %s", vm.clock.zone, off);
  detailRow(c, 0, S_DET_ZONE, v);
  // Clock
  if (!vm.clock.valid || vm.clock.q == ClockQuality::None) snprintf(v, sizeof v, "%s", str(S_DET_NOT_SET));
  else if (vm.clock.q == ClockQuality::Approx)
    snprintf(v, sizeof v, "%s", str(vm.clock.src == ClockSource::Build ? S_DET_APPROX_BUILD : S_DET_APPROX_REST));
  else if (vm.clock.syncAgeS < 60 && vm.clock.q == ClockQuality::Synced) snprintf(v, sizeof v, "%s", str(S_DET_SYNCED_NOW));
  else {
    fmtAgo(a, sizeof a, vm.clock.syncAgeS);
    snprintf(v, sizeof v, str(vm.clock.q == ClockQuality::Synced ? S_DET_SYNCED : S_DET_MANUAL), a);
  }
  detailRow(c, 1, S_DET_CLOCK, v);
  // Sensor
  const ThermoView& t = vm.thermo;
  snprintf(a, sizeof a, "0x%02X", t.addr ? t.addr : 0x44);
  if (t.chip == ShtChip::Sht4x || t.chip == ShtChip::Sht3x)
    snprintf(v, sizeof v, str(S_DET_SENSOR_OK), t.chip == ShtChip::Sht4x ? "SHT4x" : "SHT3x");
  else snprintf(v, sizeof v, str(t.chip == ShtChip::Unknown ? S_DET_UNKNOWN_AT : S_DET_NO_REPLY), a);
  detailRow(c, 2, S_DET_SENSOR, v);
  // Heater
  const int16_t hm = vm.settings ? optionValue(SettingId::Heater, vm.settings->get(SettingId::Heater)) : 0;
  if (t.chip == ShtChip::Sht3x) snprintf(v, sizeof v, "%s", str(S_DET_HEAT_NA));
  else if (t.heaterUi == 1) snprintf(v, sizeof v, "%s", str(S_DET_HEAT_ON));
  else if (t.heaterUi == 2) snprintf(v, sizeof v, "%s", str(S_DET_HEAT_COOL));
  else if (t.lastSkip == HeaterDecision::SkipHot) snprintf(v, sizeof v, "%s", str(S_DET_HEAT_HOT));
  else if (hm == 0) snprintf(v, sizeof v, "%s", str(S_DET_HEAT_OFF));
  else if (hm == 1) snprintf(v, sizeof v, "%s", str(S_DET_HEAT_AUTO));
  else {
    static const int kMin[6] = {0, 0, 5, 10, 30, 60};
    snprintf(a, sizeof a, "%dm", kMin[hm % 6]);
    snprintf(v, sizeof v, str(S_DET_HEAT_EVERY), a);
  }
  detailRow(c, 3, S_DET_HEATER, v);
  // Dew point, absolute humidity (or the compass fault)
  const bool f = useFahrenheit(vm);
  if (t.valid && !t.stale) { fmtTemp(a, sizeof a, t.dewC, f); snprintf(v, sizeof v, "%s`%c", a, f ? 'F' : 'C'); }
  else snprintf(v, sizeof v, "--");
  detailRow(c, 4, S_DET_DEW, v);
  if (vm.mag.chip == MagChip::None) {
    c.font(Font::Body);
    c.text(0, 55, str(S_DET_COMPASS_NF));
  } else {
    if (t.valid && !t.stale) snprintf(v, sizeof v, "%d.%d g/m3", (int)t.ahGm3, (int)(t.ahGm3 * 10) % 10);
    else snprintf(v, sizeof v, "--");
    detailRow(c, 5, S_DET_AH, v);
  }
}

class HomeScreen final : public Screen {
 public:
  void bindings(const ViewModel&, const UiState& ui, Bindings& b) const override {
    b.key[(int)Key::Up] = pageKey(ActionId::PagePrev);
    b.key[(int)Key::Down] = pageKey(ActionId::PageNext);
    b.key[(int)Key::Hash] = act(ActionId::DetailsToggle, ui.details ? IconId::Back : IconId::Info);
    b.key[(int)Key::Star] = holdAct(ActionId::DisplayOff, ActionId::PowerOff, kHoldCriticalMs, IconId::Sleep,
                                    IconId::Power, S_L_OFF, LabelMode::PillOnHold, 0);
  }
  void render(Canvas& c, const ViewModel& vm, const UiState& ui, MonoUs) const override {
    if (ui.details) { details(c, vm); return; }
    widgets::homeTopLine(c, vm.clock.local, vm.clock.valid, vm.clock.zone, chipsFor(vm.bg, ScreenId::Home),
                         vm.bg.syncTag);
    char hm[8], ss[4];
    c.font(Font::Hero);
    if (vm.clock.valid) {
      fmtClock(hm, sizeof hm, vm.clock.local.hour, vm.clock.local.minute);
      c.num(0, 11, hm);
      snprintf(ss, sizeof ss, "%02d", vm.clock.local.second);
      c.font(Font::Mid);
      c.num(96, 11, ss);
    } else {
      c.num(0, 11, "--:--");
    }
    widgets::battery(c, 98, 33, vm.bat, (vm.clock.local.second & 1) == 0);
    c.dottedH(0, 45, kContentX1 + 1);
    climateRow(c, vm);
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 1000; }
  uint8_t sensorDemand(const ViewModel&, const UiState&) const override { return 2; }   // fast SHT cadence
};
const HomeScreen kHome;
}  // namespace

const Screen& homeScreen() { return kHome; }

}  // namespace hg
