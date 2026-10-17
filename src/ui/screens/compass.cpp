// COMPASS (SPEC §3.8.4, §6.4)
#include <math.h>
#include <stdio.h>

#include "../../config/timing.h"
#include "../../core/compass_math.h"
#include "../canvas.h"
#include "../strings.h"
#include "../theme.h"
#include "../widgets.h"
#include "screens.h"

namespace hg {
namespace {
constexpr int16_t kCx = 31, kCy = 34, kR = 29;

float declination(const ViewModel& vm) { return vm.settings ? (float)vm.settings->get(SettingId::Declination) : 0.0f; }

void card(Canvas& c, float cardDeg) {
  for (int a = 15; a < 360; a += 15) {
    if (a % 90 == 0) continue;
    int16_t x0, y0, x1, y1;
    const float ang = (float)a - cardDeg;
    c.polar(kCx, kCy, 28, ang, x0, y0);
    c.polar(kCx, kCy, a % 45 == 0 ? 25.0f : 27.0f, ang, x1, y1);
    c.line(x0, y0, x1, y1);
  }
  static const char* const kCard[4] = {"N", "E", "S", "W"};
  c.font(Font::Label);
  for (int i = 0; i < 4; ++i) {
    int16_t x, y;
    c.polar(kCx, kCy, 21, (float)(90 * i) - cardDeg, x, y);
    if (i == 0) {
      c.disc(x, y, 5);
      c.color(0);
      c.text((int16_t)(x - 2), (int16_t)(y - 3), "N", Align::Left);
      c.color(1);
    } else {
      c.text(x, (int16_t)(y - 3), kCard[i], Align::Center);
    }
  }
}

void needle(Canvas& c, float cardDeg) {
  const float n = -cardDeg;
  int16_t tx, ty, lx, ly, rx, ry, sx, sy;
  c.polar(kCx, kCy, 15, n, tx, ty);
  c.polar(kCx, kCy, 3, n - 90, lx, ly);
  c.polar(kCx, kCy, 3, n + 90, rx, ry);
  c.triangle(tx, ty, lx, ly, rx, ry);
  c.polar(kCx, kCy, 11, n + 180, sx, sy);
  c.line(lx, ly, sx, sy);
  c.line(rx, ry, sx, sy);
  c.color(0);
  c.disc(kCx, kCy, 2);
  c.color(1);
  c.circle(kCx, kCy, 2);
}

void target(Canvas& c, float rel) {
  for (int r = 6; r <= 19; r += 3) {
    int16_t x, y;
    c.polar(kCx, kCy, (float)r, rel, x, y);
    c.pixel(x, y);
  }
  int16_t x, y;
  c.polar(kCx, kCy, 25, rel, x, y);
  c.color(0);
  c.disc(x, y, 3);
  c.color(1);
  c.triangle(x, (int16_t)(y - 3), (int16_t)(x - 3), y, (int16_t)(x + 3), y);
  c.triangle(x, (int16_t)(y + 3), (int16_t)(x - 3), y, (int16_t)(x + 3), y);
}

void sensorProblem(Canvas& c, const ViewModel& vm) {
  for (int a = 0; a < 360; a += 10) {   // ghost ring
    int16_t x, y;
    c.polar(kCx, kCy, kR, (float)a, x, y);
    c.pixel(x, y);
  }
  c.font(Font::Mid);
  c.text(kCx, 26, "?", Align::Center);
  if (vm.mag.chip == MagChip::Unknown) {
    widgets::tag(c, 66, 11, str(S_TAG_UNKNOWN));
    c.font(Font::Body);
    char a[8], t[16];
    snprintf(a, sizeof a, "0x%02X", vm.mag.addr);
    snprintf(t, sizeof t, str(S_CMP_AT), a);
    c.text(66, 22, t);
    snprintf(a, sizeof a, "0x%02X", vm.mag.idByte);
    snprintf(t, sizeof t, str(S_CMP_ID), a);
    c.text(66, 31, t);
    c.text(66, 40, str(S_CMP_SEE_DIAG));
  } else {
    widgets::tag(c, 66, 11, str(S_TAG_NOT_FOUND));
    c.font(Font::Body);
    c.text(66, 22, str(S_CMP_NO_REPLY));
    c.text(66, 31, str(S_CMP_ADDRS));
    c.text(66, 40, str(S_CMP_CHECK));
    c.text(66, 49, str(S_CMP_PINS));
  }
}

void rightColumn(Canvas& c, const ViewModel& vm) {
  const MagView& m = vm.mag;
  char t[16];
  snprintf(t, sizeof t, "%03d", m.shownDeg >= 0 ? m.shownDeg % 360 : 0);
  c.font(Font::Heading);
  const int16_t w = c.num(66, 11, t);
  c.degree((int16_t)(66 + w + 1), 11, true);
  c.font(Font::Label);
  c.text(67, 37, point16((float)m.shownDeg));
  // Line A (ink 48)
  c.font(Font::Body);
  if (m.saturated) widgets::tag(c, 66, 47, str(S_TAG_SATURATED));
  else if (m.fieldSuspect) widgets::tag(c, 66, 47, str(S_TAG_FIELD));
  else if (!m.calibrated) widgets::tag(c, 66, 47, str(S_TAG_UNCAL));
  else if (vm.target.set) {
    char b[12];
    snprintf(b, sizeof b, "%03d`", vm.target.bearing);
    snprintf(t, sizeof t, str(S_CMP_TGT), b);
    c.text(66, 48, t);
  } else {
    const int d = (int)declination(vm);
    if (d) {
      char b[12];
      snprintf(b, sizeof b, d > 0 ? "+%d`" : "%d`", d);
      snprintf(t, sizeof t, str(S_CMP_TRUE), b);
      c.text(66, 48, t);
    } else {
      c.text(66, 48, str(S_CMP_MAG));
    }
  }
  // Line B (ink 57; the Label steer number at ink 56: its 8 caps rows must end on row 63)
  c.font(Font::Body);
  if (m.fieldSuspect) { c.text(66, 57, str(S_CMP_METAL)); return; }
  if (!vm.target.set) { c.text(66, 57, str(S_CMP_HOLD_LEVEL)); return; }
  const int steer = (int)lroundf(vm.target.steerDeg);
  if (steer >= -3 && steer <= 3) { c.text(66, 57, str(S_CMP_ON_COURSE)); return; }
  snprintf(t, sizeof t, "%d`", steer < 0 ? -steer : steer);
  c.font(Font::Label);
  if (steer > 0) {           // turn right: the triangle after the number
    const int16_t w2 = c.num(66, 56, t);
    const int16_t x = (int16_t)(66 + w2 + 2);
    c.triangle(x, 56, x, 62, (int16_t)(x + 4), 59);
  } else {                   // turn left: before it
    c.triangle(70, 56, 70, 62, 66, 59);
    c.num(73, 56, t);
  }
}

class CompassScreen final : public Screen {
 public:
  void bindings(const ViewModel& vm, const UiState&, Bindings& b) const override {
    b.key[(int)Key::Up] = pageKey(ActionId::PagePrev);
    b.key[(int)Key::Down] = pageKey(ActionId::PageNext);
    if (vm.mag.chip == MagChip::None || vm.mag.chip == MagChip::Unknown) {
      b.key[(int)Key::Star] = act(ActionId::Rescan, IconId::Retry);
      return;
    }
    b.key[(int)Key::Hash] = act(ActionId::CmpCalibrate, IconId::Cal, vm.mag.calibrated ? 0 : S_L_CAL, LabelMode::Static);
    if (vm.target.set)
      b.key[(int)Key::Star] = holdAct(ActionId::ShowHint, ActionId::CmpClear, kHoldMs, IconId::X, IconId::None, S_L_HOLD,
                                      LabelMode::PillOnHold, S_HINT_HOLD_CLEAR);
    else
      b.key[(int)Key::Star] = act(ActionId::CmpMark, IconId::Target);
  }
  void render(Canvas& c, const ViewModel& vm, const UiState&, MonoUs) const override {
    c.triangle(28, 0, 34, 0, 31, 3);   // lubber line
    const ChipSet chips = chipsFor(vm.bg, ScreenId::Compass);
    c.scope(DrawScope::Status);
    widgets::chips(c, kContentX1, 66, 1, 1, chips);
    c.scope(DrawScope::Content);
    if (vm.mag.chip == MagChip::None || vm.mag.chip == MagChip::Unknown) { sensorProblem(c, vm); return; }
    c.circle(kCx, kCy, kR);
    card(c, vm.mag.cardDeg);
    if (vm.target.set) target(c, (float)vm.target.bearing - vm.mag.cardDeg);
    needle(c, vm.mag.cardDeg);
    rightColumn(c, vm);
  }
  uint16_t framePeriodMs(const ViewModel& vm, const UiState&) const override {
    return fabsf(wrap180(vm.mag.cardDeg - vm.mag.headingDeg)) < 0.3f ? 200 : 40;   // 25 fps until settled, then 5
  }
  uint8_t sensorDemand(const ViewModel&, const UiState&) const override { return 1; }
};
const CompassScreen kCompass;
}  // namespace

const Screen& compassScreen() { return kCompass; }

}  // namespace hg
