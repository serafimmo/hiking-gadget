// STOPWATCH (SPEC §3.8.2)
#include <stdio.h>

#include "../../config/timing.h"
#include "../canvas.h"
#include "../fmt.h"
#include "../strings.h"
#include "../theme.h"
#include "../widgets.h"
#include "screens.h"

namespace hg {
namespace {
void bigTime(Canvas& c, int64_t us) {
  const int64_t s = us / kUsPerS;
  char main[16], frac[8];
  if (s < 3600) {
    fmtMmSs(main, sizeof main, s);
    c.font(Font::Big);
    const int16_t w = c.num(0, 12, main);
    snprintf(frac, sizeof frac, ".%02d", (int)(us / 10000 % 100));
    c.font(Font::Mid);
    c.num((int16_t)(w + 1), 24, frac);
  } else if (s < 36000) {
    fmtHMmSs(main, sizeof main, s);
    c.font(Font::BigS);
    const int16_t w = c.num(0, 14, main);
    snprintf(frac, sizeof frac, ".%d", (int)(us / 100000 % 10));
    c.font(Font::Mid);
    c.num((int16_t)(w + 1), 22, frac);
  } else {
    snprintf(main, sizeof main, "%02d:%02d:%02d", (int)(s / 3600 % 100), (int)(s / 60 % 60), (int)(s % 60));
    c.font(Font::BigS);
    c.num(0, 14, main);
  }
}

// lapNo 0 = no number (the segment after lap 999 cannot be recorded, so it has none)
void lapRow(Canvas& c, int16_t ink, unsigned lapNo, bool live, int64_t lapUs, bool delta, int64_t deltaUs) {
  char t[16];
  c.font(Font::Body);
  if (lapNo) {
    snprintf(t, sizeof t, "L%u", lapNo);
    c.num(0, ink, t);
  }
  if (live) c.icon(24, (int16_t)(ink + 1), IconId::Live);
  fmtLapTime(t, sizeof t, lapUs);
  c.num(29, ink, t);
  if (delta) {
    fmtLapDelta(t, sizeof t, deltaUs);
    c.num(kContentX1, ink, t, Align::Right);
  }
}

class StopwatchScreen final : public Screen {
 public:
  void bindings(const ViewModel& vm, const UiState&, Bindings& b) const override {
    b.key[(int)Key::Up] = pageKey(ActionId::PagePrev);
    b.key[(int)Key::Down] = pageKey(ActionId::PageNext);
    switch (vm.sw.st) {
      case Stopwatch::State::Idle:
        b.key[(int)Key::Star] = act(ActionId::SwStart, IconId::Play, S_L_START, LabelMode::Static);
        break;
      case Stopwatch::State::Running:
        b.key[(int)Key::Hash] = act(ActionId::SwLap, IconId::Flag);
        b.key[(int)Key::Star] = act(ActionId::SwPause, IconId::Pause);
        break;
      case Stopwatch::State::Paused:
        b.key[(int)Key::Hash] = holdAct(ActionId::ShowHint, ActionId::SwReset, kHoldMs, IconId::Reset, IconId::None,
                                        S_L_HOLD, LabelMode::Static, S_HINT_HOLD_RESET);
        b.key[(int)Key::Star] = act(ActionId::SwResume, IconId::Play);
        break;
    }
  }
  void render(Canvas& c, const ViewModel& vm, const UiState&, MonoUs) const override {
    char clk[8];
    statusClock(vm, clk, sizeof clk);
    const ChipSet chips = chipsFor(vm.bg, ScreenId::Stopwatch);
    widgets::statusLine(c, str(S_SW_TITLE), vm.sw.st == Stopwatch::State::Paused ? str(S_TAG_PAUSED) : nullptr, clk, &chips);
    bigTime(c, vm.sw.elapsedUs);
    // The rule stops 3 px before a static keycap label (HOLD), whose box must stay free of content ink.
    int16_t ruleEnd = kContentX1;
    if (vm.sw.st == Stopwatch::State::Paused) {
      c.font(Font::Body);
      ruleEnd = (int16_t)(kLabelRight - c.width(str(S_L_HOLD)) - 1 - 3);
    }
    c.dottedH(0, 43, (int16_t)(ruleEnd + 1));
    if (vm.sw.st == Stopwatch::State::Idle) {
      c.font(Font::Body);
      c.text(0, 47, str(S_SW_READY));
      return;
    }
    const unsigned cur = vm.sw.lapNo >= Stopwatch::kMaxLapNo ? 0u : vm.sw.lapNo + 1u;
    lapRow(c, 47, cur, vm.sw.st == Stopwatch::State::Running, vm.sw.curLapUs, false, 0);
    if (vm.sw.lapNo) lapRow(c, 56, vm.sw.lapNo, false, vm.sw.lastLapUs, vm.sw.laps >= 2, vm.sw.lastDeltaUs);
  }
  uint16_t framePeriodMs(const ViewModel& vm, const UiState&) const override {
    return vm.sw.st == Stopwatch::State::Running ? 50 : 0;
  }
};
const StopwatchScreen kStopwatch;
}  // namespace

const Screen& stopwatchScreen() { return kStopwatch; }

}  // namespace hg
