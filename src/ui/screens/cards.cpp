// Cards (SPEC §3.8.8): CHECK HARDWARE, I2C BUS STUCK, KEY STUCK, STATE/TIMER LOST, SETTINGS RESET, SHT3x info.
#include <stdio.h>

#include "../../config/board_pins.h"
#include "../../core/health.h"
#include "../canvas.h"
#include "../fmt.h"
#include "../strings.h"
#include "../theme.h"
#include "../widgets.h"
#include "screens.h"

namespace hg {
namespace {
constexpr int16_t kRows[4] = {15, 24, 33, 42};

void code(Canvas& c, ErrCode e) {
  c.font(Font::Body);
  c.text(0, 56, errInfo(e).mnemonic);
}

void fourRows(Canvas& c, const uint16_t* ids, const char* arg0) {
  char t[48];
  c.font(Font::Body);
  for (int i = 0; i < 4; ++i) {
    if (!ids[i]) continue;
    snprintf(t, sizeof t, str(ids[i]), i == 0 ? arg0 : "");
    c.text(0, kRows[i], t);
  }
}

void keyName(char* b, size_t n, uint8_t k) { snprintf(b, n, "K%u (GPIO%u)", k + 1, kKeyPins[k & 3]); }

struct Sub { uint16_t okStr; ErrCode err; uint8_t addr; };

void checkHardware(Canvas& c, const ViewModel& vm) {
  widgets::cardHeader(c, str(S_CARD_CHECK_HW));
  c.scope(DrawScope::Card);
  const Sub subs[4] = {{S_CHK_I2C_OK, vm.sys.busErr, 0},
                       {S_CHK_TEMP_OK, vm.thermo.err, vm.thermo.addr ? vm.thermo.addr : (uint8_t)0x44},
                       {S_CHK_COMPASS_OK, vm.mag.err, vm.mag.addr},
                       {S_CHK_STORAGE_OK, vm.sys.nvsErr, 0}};
  auto failed = [](const Sub& s) { return s.err != ErrCode::None && errInfo(s.err).sev >= Severity::Fault; };
  int nFail = 0;
  for (const Sub& s : subs) nFail += failed(s);
  const int16_t ys[4] = {14, 23, 32, 41};
  int slot = 0, shownFail = 0;
  ErrCode first = ErrCode::None;
  char t[48], a[16];
  c.font(Font::Body);
  for (const Sub& s : subs) {
    if (!failed(s)) continue;
    const bool lastSlotNeeded = nFail - shownFail > 1 && slot == 3;
    if (slot > 3 || lastSlotNeeded) break;
    snprintf(a, sizeof a, "0x%02X", s.addr);
    snprintf(t, sizeof t, str(errTitle(s.err)), a);
    c.icon(0, (int16_t)(ys[slot] + 1), IconId::SFail);
    c.text(8, ys[slot], t);
    ++slot;
    ++shownFail;
    if (first == ErrCode::None) {
      first = s.err;
      if (slot <= 3) { c.text(8, ys[slot], str(errHint(s.err))); ++slot; }
    }
  }
  if (shownFail < nFail) {
    snprintf(a, sizeof a, "%d", nFail - shownFail);
    snprintf(t, sizeof t, str(S_CHK_MORE), a);
    c.text(8, ys[3], t);
    slot = 4;
  }
  for (const Sub& s : subs) {
    if (failed(s) || slot > 3) continue;
    c.icon(0, (int16_t)(ys[slot] + 1), IconId::SOk);
    c.text(8, ys[slot], str(s.okStr));
    ++slot;
  }
  if (first != ErrCode::None) code(c, first);
}

class CardScreen final : public Screen {
 public:
  void bindings(const ViewModel&, const UiState&, Bindings& b) const override {
    b.key[(int)Key::Star] = act(ActionId::CardDismiss, IconId::Check, S_L_OK, LabelMode::Static);
  }
  void render(Canvas& c, const ViewModel& vm, const UiState& ui, MonoUs) const override {
    char t[48], a[16];
    switch (ui.card) {
      case ErrCode::I2cStuck: {
        widgets::cardHeader(c, str(S_CARD_I2C_STUCK));
        const uint16_t r[4] = {S_I2C_1, S_I2C_2, S_I2C_3, S_I2C_4};
        fourRows(c, r, "");
        code(c, ErrCode::I2cStuck);
        break;
      }
      case ErrCode::KeyStuck: {
        const uint8_t k = ui.cardArg & 3, variant = (uint8_t)(ui.cardArg >> 4);
        snprintf(t, sizeof t, str(S_CARD_KEY_STUCK), k == 1 ? "V" : keySymbol(k));   // Label is caps-only
        widgets::cardHeader(c, t);
        keyName(a, sizeof a, k);
        const uint16_t rt[4] = {S_KEY_RT_1, S_KEY_RT_2, S_KEY_RT_3, S_KEY_RT_4};
        const uint16_t sl[4] = {S_KEY_SL_1, S_KEY_SL_2, S_KEY_SL_3, S_KEY_SL_4};
        const uint16_t ph[4] = {S_KEY_PH_1, S_KEY_PH_2, S_KEY_PH_3, S_KEY_PH_4};
        fourRows(c, variant == 1 ? sl : variant == 2 ? ph : rt, a);
        code(c, ErrCode::KeyStuck);
        break;
      }
      case ErrCode::RtcLost:
      case ErrCode::CrashRestored: {
        const bool loop = ui.cardArg == 2 || ui.card == ErrCode::CrashRestored;
        widgets::cardHeader(c, str(ui.cardArg == 1 ? S_CARD_TIMER_LOST : S_CARD_STATE_LOST));
        const uint16_t lost[4] = {S_LOST_1, S_LOST_2, S_LOST_3, S_LOST_4};
        const uint16_t crash[4] = {S_LOOP_1, S_LOOP_2, S_LOST_2, S_LOST_3};
        fourRows(c, loop ? crash : lost, "");
        code(c, loop ? ErrCode::CrashRestored : ErrCode::RtcLost);
        break;
      }
      case ErrCode::CfgReset: {
        widgets::cardHeader(c, str(S_CARD_CFG_RESET));
        const SettingId id = (SettingId)(ui.cardArg < (uint8_t)SettingId::Count ? ui.cardArg : 0);
        c.font(Font::Body);
        snprintf(t, sizeof t, str(S_CFG_1), str(settingLabel(id)));
        c.text(0, kRows[0], t);
        char v[24];
        formatSettingValue(id, settingDef(id).def, ValueStyle::List, vm.fmt, v, sizeof v);
        snprintf(t, sizeof t, str(S_CFG_2), v);
        c.text(0, kRows[1], t);
        code(c, ErrCode::CfgReset);
        break;
      }
      case ErrCode::Sht3xInfo: {
        widgets::cardHeader(c, str(S_CARD_SHT3X));
        const uint16_t r[4] = {S_SHT3X_1, S_SHT3X_2, S_SHT3X_3, S_SHT3X_4};
        fourRows(c, r, "");
        code(c, ErrCode::Sht3xInfo);
        break;
      }
      default:
        checkHardware(c, vm);
        break;
    }
    c.scope(DrawScope::Content);
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 0; }
};
const CardScreen kCard;

class PowerOffScreen final : public Screen {
 public:
  void bindings(const ViewModel&, const UiState&, Bindings&) const override {}
  void render(Canvas& c, const ViewModel& vm, const UiState&, MonoUs) const override {
    c.scope(DrawScope::Overlay);
    c.circle(64, 22, 10);
    c.color(0);
    c.box(Rect{61, 10, 7, 8});
    c.color(1);
    c.box(Rect{63, 8, 3, 12});
    c.font(Font::Label);
    c.text(64, 38, str(S_PWR_TITLE), Align::Center);
    c.font(Font::Body);
    c.text(64, 49, str(S_PWR_WAKE), Align::Center);
    if (vm.tm.st == Countdown::State::Running) c.text(64, 57, str(S_PWR_TIMER), Align::Center);
    else if (vm.sw.st == Stopwatch::State::Running) c.text(64, 57, str(S_PWR_SW), Align::Center);
    c.scope(DrawScope::Content);
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 0; }
};
const PowerOffScreen kPowerOff;

class SplashScreen final : public Screen {
 public:
  void bindings(const ViewModel&, const UiState&, Bindings& b) const override { b.railHidden = true; }
  void render(Canvas& c, const ViewModel& vm, const UiState&, MonoUs) const override {
    c.scope(DrawScope::Overlay);
    static const int16_t kPts[11][2] = {{0, 44}, {14, 34}, {22, 38}, {38, 18}, {48, 28}, {58, 22},
                                        {76, 40}, {88, 30}, {100, 36}, {116, 26}, {127, 32}};
    for (int i = 0; i + 1 < 11; ++i) c.line(kPts[i][0], kPts[i][1], kPts[i + 1][0], kPts[i + 1][1]);
    c.disc(38, 18, 1);
    c.font(Font::Value);
    c.text(64, 47, str(S_SPLASH_NAME), Align::Center);
    char v[16];
    snprintf(v, sizeof v, "V%s", vm.sys.fw ? vm.sys.fw : "?");
    c.font(Font::Micro);
    c.text(64, 59, v, Align::Center);
    c.scope(DrawScope::Content);
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 0; }
};
const SplashScreen kSplash;
}  // namespace

const Screen& cardScreen() { return kCard; }
const Screen& powerOffScreen() { return kPowerOff; }
const Screen& splashScreen() { return kSplash; }

}  // namespace hg
