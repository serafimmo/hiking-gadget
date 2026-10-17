// TIMER (SPEC §3.8.3) and the ALERT overlay
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
void rightColumn(Canvas& c, const ViewModel& vm, int64_t totalUs) {
  char t[16];
  fmtTimerTotal(t, sizeof t, totalUs);
  c.font(Font::Body);
  c.text(kContentX1, 12, t, Align::Right);
  if (vm.clock.valid && vm.tm.endsKnown) {
    c.text(kContentX1, 23, str(S_TM_ENDS), Align::Right);
    fmtClock(t, sizeof t, vm.tm.endsLocal.hour, vm.tm.endsLocal.minute);
    c.num(kContentX1, 32, t, Align::Right);
  }
}

void presetChips(Canvas& c, uint8_t sel) {
  char t[8];
  for (uint8_t i = 0; i < Countdown::kPresetCount; ++i) {
    snprintf(t, sizeof t, "%u", Countdown::kPresetMin[i]);
    c.font(Font::Label);
    const int16_t cx = (int16_t)(19 * i + 9);
    if (i == sel) {
      c.rbox(Rect{(int16_t)(19 * i), 50, 17, 12}, 2);
      c.color(0);
      c.text(cx, 52, t, Align::Center);
      c.color(1);
    } else {
      c.text(cx, 52, t, Align::Center);
    }
  }
}

void runningView(Canvas& c, const ViewModel& vm, bool digitsOn) {
  const int64_t s = Countdown::displaySeconds(vm.tm.remainingUs);
  char t[16];
  if (s >= 3600) {
    fmtHMmSs(t, sizeof t, s);
    c.font(Font::BigS);
    if (digitsOn) c.num(0, 14, t);
    char tot[16], line[40];
    fmtTimerTotalLower(tot, sizeof tot, vm.tm.totalUs);
    if (vm.clock.valid && vm.tm.endsKnown) {
      char e[8];
      fmtClock(e, sizeof e, vm.tm.endsLocal.hour, vm.tm.endsLocal.minute);
      snprintf(line, sizeof line, "%s  ends %s", tot, e);
    } else {
      snprintf(line, sizeof line, "%s", tot);
    }
    c.font(Font::Body);
    c.text(0, 45, line);
  } else {
    fmtMmSs(t, sizeof t, s);
    c.font(Font::Hero);
    if (digitsOn) c.num(0, 11, t);
    rightColumn(c, vm, vm.tm.totalUs);
  }
  widgets::progressBar(c, 56, vm.tm.totalUs > 0 ? (float)vm.tm.remainingUs / (float)vm.tm.totalUs : 0.0f);
}

void doneView(Canvas& c, const ViewModel& vm) {
  char tot[16], e[8], line[40], t[16];
  fmtTimerTotalLower(tot, sizeof tot, vm.tm.totalUs);
  if (vm.clock.valid && vm.tm.endsKnown) {
    fmtClock(e, sizeof e, vm.tm.endsLocal.hour, vm.tm.endsLocal.minute);
    snprintf(line, sizeof line, "%s timer, ended %s", tot, e);
  } else {
    snprintf(line, sizeof line, "%s timer, ended", tot);
  }
  c.font(Font::Body);
  c.text(0, 12, line);
  int64_t s = vm.tm.overtimeUs / kUsPerS;
  if (s > 359999) s = 359999;   // the display stops at 99:59:59
  if (s < 3600) {
    snprintf(t, sizeof t, "+%02d:%02d", (int)(s / 60), (int)(s % 60));
    c.font(Font::BigS);
    c.num(0, 22, t);
  } else {
    snprintf(t, sizeof t, "+%d:%02d:%02d", (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
    c.font(Font::Mid);
    c.num(0, 26, t);
  }
  c.font(Font::Body);
  c.text(0, 50, str(S_TM_OVERTIME));
}

class TimerScreen final : public Screen {
 public:
  void bindings(const ViewModel& vm, const UiState&, Bindings& b) const override {
    b.key[(int)Key::Up] = pageKey(ActionId::PagePrev);
    b.key[(int)Key::Down] = pageKey(ActionId::PageNext);
    switch (vm.tm.st) {
      case Countdown::State::Select:
        b.key[(int)Key::Hash] = act(ActionId::TmNextPreset, IconId::Next);
        b.key[(int)Key::Star] = act(ActionId::TmStart, IconId::Play);
        break;
      case Countdown::State::Running:
        b.key[(int)Key::Hash] = act(ActionId::TmAddMinute, IconId::Plus, S_L_PLUS1, LabelMode::Static);
        b.key[(int)Key::Star] = act(ActionId::TmPause, IconId::Pause);
        break;
      case Countdown::State::Paused:
        b.key[(int)Key::Hash] = holdAct(ActionId::ShowHint, ActionId::TmCancel, kHoldMs, IconId::X, IconId::None,
                                        S_L_HOLD, LabelMode::Static, S_HINT_HOLD_CANCEL);
        b.key[(int)Key::Star] = act(ActionId::TmResume, IconId::Play);
        break;
      case Countdown::State::Done:
        b.key[(int)Key::Hash] = act(ActionId::TmNew, IconId::X, S_L_NEW, LabelMode::Static);
        b.key[(int)Key::Star] = act(ActionId::TmRestart, IconId::Reset, S_L_AGAIN, LabelMode::Static);
        break;
    }
  }
  void render(Canvas& c, const ViewModel& vm, const UiState&, MonoUs now) const override {
    char clk[8];
    statusClock(vm, clk, sizeof clk);
    const ChipSet chips = chipsFor(vm.bg, ScreenId::Timer);
    const char* tagWord = vm.tm.st == Countdown::State::Paused ? str(S_TAG_PAUSED)
                        : vm.tm.st == Countdown::State::Done ? str(S_TAG_DONE) : nullptr;
    widgets::statusLine(c, str(S_TM_TITLE), tagWord, clk, &chips);
    switch (vm.tm.st) {
      case Countdown::State::Select: {
        char t[16];
        fmtMmSs(t, sizeof t, Countdown::displaySeconds(vm.tm.totalUs));
        c.font(Font::Hero);
        c.num(0, 11, t);
        rightColumn(c, vm, vm.tm.totalUs);
        presetChips(c, vm.tm.preset);
        break;
      }
      case Countdown::State::Running:
        runningView(c, vm, true);
        break;
      case Countdown::State::Paused:
        runningView(c, vm, (now.v / 1000 % 1000) < 700);   // 1 Hz, 700 ms on / 300 ms off (§3.6)
        break;
      case Countdown::State::Done:
        doneView(c, vm);
        break;
    }
  }
  uint16_t framePeriodMs(const ViewModel& vm, const UiState&) const override {
    switch (vm.tm.st) {
      case Countdown::State::Running: return 100;
      case Countdown::State::Paused:  return 100;
      case Countdown::State::Done:    return 1000;
      default:                        return 0;
    }
  }
};
const TimerScreen kTimer;

class AlertScreen final : public Screen {
 public:
  void bindings(const ViewModel&, const UiState&, Bindings& b) const override {
    for (int k = 0; k < kKeyCount; ++k) b.key[k] = pageKey(ActionId::AlertDismiss);   // any key, on Down
    b.railHidden = true;
  }
  void render(Canvas& c, const ViewModel& vm, const UiState&, MonoUs) const override {
    c.scope(DrawScope::Overlay);
    c.icon(3, 3, IconId::Bell);
    c.font(Font::Label);
    c.text(13, 3, str(S_ALERT_TITLE));
    char t[16];
    fmtTimerTotalLower(t, sizeof t, vm.tm.totalUs);
    c.font(Font::Body);
    c.text(124, 3, t, Align::Right);
    int64_t s = vm.tm.overtimeUs / kUsPerS;
    if (s > 5999) s = 5999;
    snprintf(t, sizeof t, "+%d:%02d", (int)(s / 60), (int)(s % 60));
    c.font(Font::Hero);
    c.num(64, 16, t, Align::Center);
    c.font(Font::Body);
    c.text(64, 54, str(S_ANY_KEY_OK), Align::Center);
    c.scope(DrawScope::Content);
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 1000; }
};
const AlertScreen kAlert;
}  // namespace

const Screen& timerScreen() { return kTimer; }
const Screen& alertScreen() { return kAlert; }

}  // namespace hg
