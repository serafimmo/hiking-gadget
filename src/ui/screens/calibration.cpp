// CALIBRATION (SPEC §3.8.6)
#include <math.h>
#include <stdio.h>

#include "../canvas.h"
#include "../strings.h"
#include "../theme.h"
#include "../widgets.h"
#include "screens.h"

namespace hg {
namespace {
constexpr int16_t kCx = 31, kCy = 33;

uint16_t hintStr(MagCalibrator::Hint h) {
  switch (h) {
    case MagCalibrator::Hint::KeepLevel:  return S_CAL_H_LEVEL;
    case MagCalibrator::Hint::TurnSlower: return S_CAL_H_SLOWER;
    case MagCalibrator::Hint::MetalNear:  return S_CAL_H_METAL;
    case MagCalibrator::Hint::FieldWeak:  return S_CAL_H_WEAK;
    case MagCalibrator::Hint::Ready:      return S_CAL_H_READY;
    default:                              return S_CAL_H_MORE;
  }
}

bool resultShown(const ViewModel& vm) {
  return vm.cal.haveResult || vm.cal.p.verdict == MagCalibrator::Verdict::Failed;
}
bool resultGood(const ViewModel& vm) { return vm.cal.haveResult && vm.cal.r.grade != MagCalibrator::Grade::Poor; }

void collecting(Canvas& c, const ViewModel& vm) {
  const MagCalibrator::Progress& p = vm.cal.p;
  for (int i = 0; i < 24; ++i) {
    if (p.binsMask & (1u << i)) {
      for (float a = 15.0f * i + 1.0f; a <= 15.0f * i + 14.0f; a += 1.5f) {
        int16_t x0, y0, x1, y1;
        c.polar(kCx, kCy, 27, a, x0, y0);
        c.polar(kCx, kCy, 23, a, x1, y1);
        c.line(x0, y0, x1, y1);
      }
    } else {
      int16_t x, y;
      c.polar(kCx, kCy, 25, 15.0f * i + 7.5f, x, y);
      c.pixel(x, y);
    }
  }
  int16_t mx, my;
  c.polar(kCx, kCy, 19, p.liveDeg, mx, my);
  c.disc(mx, my, 1);
  char t[16];
  snprintf(t, sizeof t, "%u%%", p.coveragePct);
  c.font(Font::Mid);
  c.text(kCx, 26, t, Align::Center);
  c.font(Font::Body);
  c.text(64, 2, str(S_CAL_TITLE));
  c.text(64, 14, str(S_CAL_KEEP));
  c.text(64, 23, str(S_CAL_TURN));
  c.text(64, 32, str(S_CAL_TWO));
  char f[8];
  snprintf(f, sizeof f, "%d", (int)lroundf(p.fieldUt));
  snprintf(t, sizeof t, str(S_CAL_FIELD), f);
  c.text(64, 41, t);
  c.text(64, 50, str(p.verdict == MagCalibrator::Verdict::Ready ? (uint16_t)S_CAL_H_READY : hintStr(p.hint)));
}

void result(Canvas& c, const ViewModel& vm) {
  char t[40], a[24];
  if (resultGood(vm)) {
    const MagCalibrator::Result& r = vm.cal.r;
    c.disc(16, 20, 9);
    c.color(0);
    c.line(12, 20, 15, 23);
    c.line(15, 23, 21, 17);
    c.color(1);
    c.font(Font::Label);
    c.text(32, 12, str(S_CAL_DONE));
    c.font(Font::Body);
    snprintf(a, sizeof a, "%s %u%%", r.grade == MagCalibrator::Grade::Good ? "good" : "ok", r.cal.qualityPct);
    snprintf(t, sizeof t, str(S_CAL_QUALITY), a);
    c.text(32, 24, t);
    c.dottedH(0, 35, kContentX1 + 1);
    snprintf(a, sizeof a, "%+d  Y %+d", (int)lroundf(r.cal.offX), (int)lroundf(r.cal.offY));
    snprintf(t, sizeof t, str(S_CAL_OFFSET), a);
    c.text(0, 38, t);
    snprintf(a, sizeof a, "%.2f  Y %.2f", (double)r.cal.scaleX, (double)r.cal.scaleY);
    snprintf(t, sizeof t, str(S_CAL_SCALE), a);
    c.text(0, 47, t);
    snprintf(a, sizeof a, "%d", (int)lroundf(r.fieldUt));
    snprintf(t, sizeof t, str(S_CAL_FIELD), a);
    c.text(0, 56, t);
    return;
  }
  c.circle(16, 20, 9);
  c.font(Font::Label);
  c.text(16, 16, "!", Align::Center);
  c.text(32, 12, str(S_CAL_FAILED));
  c.font(Font::Body);
  uint8_t covered = 0;
  for (uint32_t m = vm.cal.p.binsMask; m; m &= m - 1) ++covered;   // exact, not from the rounded percentage
  const MagCalibrator::Hint why = vm.cal.haveResult ? vm.cal.r.reason : vm.cal.p.hint;
  if (why == MagCalibrator::Hint::FieldWeak || why == MagCalibrator::Hint::MetalNear) {
    c.text(32, 24, str(S_CAL_WHY_FIELD));
  } else {
    snprintf(a, sizeof a, "%u", covered);
    snprintf(t, sizeof t, str(S_CAL_COVERED), a);
    c.text(32, 24, t);
  }
  // ink 37/46/55 (not 38/47/56): "slowly, kept level." and "Old calibration kept" have descenders
  c.text(0, 37, str(S_CAL_TIP1));
  c.text(0, 46, str(S_CAL_TIP2));
  c.text(0, 55, str(S_CAL_TIP3));
}

class CalibrationScreen final : public Screen {
 public:
  void bindings(const ViewModel& vm, const UiState&, Bindings& b) const override {
    if (resultShown(vm)) {
      if (resultGood(vm)) {
        b.key[(int)Key::Star] = act(ActionId::CalDone, IconId::Check, S_L_OK, LabelMode::Static);
      } else {
        b.key[(int)Key::Hash] = act(ActionId::CalCancel, IconId::Back);
        b.key[(int)Key::Star] = act(ActionId::CalRetry, IconId::Retry, S_L_AGAIN, LabelMode::Static);
      }
      return;
    }
    b.key[(int)Key::Hash] = act(ActionId::CalCancel, IconId::X);
    if (vm.cal.p.verdict == MagCalibrator::Verdict::Ready)
      b.key[(int)Key::Star] = act(ActionId::CalSave, IconId::Check, S_L_SAVE, LabelMode::Static);
  }
  void render(Canvas& c, const ViewModel& vm, const UiState&, MonoUs) const override {
    if (resultShown(vm)) result(c, vm);
    else collecting(c, vm);
  }
  uint16_t framePeriodMs(const ViewModel& vm, const UiState&) const override { return resultShown(vm) ? 0 : 50; }
  uint8_t sensorDemand(const ViewModel& vm, const UiState&) const override { return resultShown(vm) ? 0 : 1; }
};
const CalibrationScreen kCalibration;
}  // namespace

const Screen& calibrationScreen() { return kCalibration; }

}  // namespace hg
