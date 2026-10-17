#include "widgets.h"

#include <math.h>
#include <string.h>

#include "fmt.h"
#include "strings.h"
#include "theme.h"

namespace hg {

ChipSet chipsFor(const BgChips& bg, ScreenId page) {
  ChipSet s{bg.warn, bg.bell, bg.tmr, bg.sw, bg.tmrRemainS};
  if (page == ScreenId::Stopwatch) s.sw = false;
  if (page == ScreenId::Timer) { s.tmr = false; s.bell = false; }
  return s;
}

namespace widgets {
namespace {
constexpr int16_t kChipGap = 3, kIconW = 5;

int16_t timerChipWidth(Canvas& c, int32_t remainS, char* text, size_t n) {
  fmtTimerChip(text, n, remainS);
  c.font(Font::Micro);
  return (int16_t)(kIconW + 1 + c.numWidth(text));
}

// Draws text in colour 0 on whatever is under it, restoring colour 1.
int16_t inverseText(Canvas& c, int16_t x, int16_t ink, const char* s, Align a = Align::Left) {
  c.color(0);
  const int16_t w = c.text(x, ink, s, a);
  c.color(1);
  return w;
}
}  // namespace

void rule(Canvas& c, int16_t y) { c.dottedH(0, y, kContentX1 + 1); }

int16_t tagWidth(Canvas& c, const char* word) {
  c.font(Font::Micro);
  return (int16_t)(c.width(word) + 3);
}

int16_t tag(Canvas& c, int16_t x, int16_t y, const char* word) {
  const int16_t w = tagWidth(c, word);
  c.rbox(Rect{x, y, w, 7}, 1);
  c.font(Font::Micro);
  inverseText(c, (int16_t)(x + 2), (int16_t)(y + 1), word);
  return w;
}

int16_t chipsWidth(Canvas& c, const ChipSet& s) {
  char t[8];
  int16_t w = 0;
  int n = 0;
  if (s.warn) { w += kIconW; ++n; }
  if (s.bell) { w += kIconW; ++n; }
  if (s.tmr) { w += timerChipWidth(c, s.tmrRemainS, t, sizeof t); ++n; }
  if (s.sw) { w += kIconW; ++n; }
  return (int16_t)(w + (n > 1 ? (n - 1) * kChipGap : 0));
}

int16_t chips(Canvas& c, int16_t xRight, int16_t L, int16_t iconY, int16_t textInk, const ChipSet& s) {
  int16_t x = xRight;          // right edge (inclusive) of the next chip
  int16_t left = (int16_t)(xRight + 1);
  for (int i = 0; i < 4; ++i) {
    char t[8] = {};
    int16_t w;
    if (i == 0) { if (!s.warn) continue; w = kIconW; }
    else if (i == 1) { if (!s.bell) continue; w = kIconW; }
    else if (i == 2) { if (!s.tmr) continue; w = timerChipWidth(c, s.tmrRemainS, t, sizeof t); }
    else { if (!s.sw) continue; w = kIconW; }
    const int16_t x0 = (int16_t)(x - w + 1);
    if (x0 < L && i > 0) break;   // this and every lower-priority chip are dropped (D30); the warning always fits
    switch (i) {
      case 0: c.icon(x0, iconY, IconId::SWarn); break;
      case 1: c.icon(x0, iconY, IconId::SBell5); break;
      case 2:
        c.icon(x0, iconY, IconId::STmr);
        c.font(Font::Micro);
        c.num((int16_t)(x0 + kIconW + 1), textInk, t);
        break;
      default: c.icon(x0, iconY, IconId::SSw); break;
    }
    left = x0;
    x = (int16_t)(x0 - 1 - kChipGap);
  }
  return left;
}

void statusLine(Canvas& c, const char* title, const char* tagWord, const char* clock, const ChipSet* chipSet) {
  c.scope(DrawScope::Status);
  c.font(Font::Micro);
  char t[24];
  strncpy(t, title ? title : "", sizeof t - 1);
  t[sizeof t - 1] = 0;
  toUpper(t);
  const int16_t tw = c.text(0, kStatusInk, t);
  int16_t rightEnd = (int16_t)(tw - 1);
  if (tagWord && *tagWord) {
    const int16_t x = (int16_t)(tw + 4);
    rightEnd = (int16_t)(x + tag(c, x, 0, tagWord) - 1);
  }
  int16_t xRight = kContentX1;
  if (clock && *clock) {
    c.font(Font::Micro);
    const int16_t cw = c.numWidth(clock);
    c.num(kContentX1, kStatusInk, clock, Align::Right);
    xRight = (int16_t)(kContentX1 - cw - kChipGap);
  }
  if (chipSet) chips(c, xRight, (int16_t)(rightEnd + 1 + kChipGap), 1, kStatusInk, *chipSet);
  rule(c, kStatusRuleY);
  c.scope(DrawScope::Content);
}

void homeTopLine(Canvas& c, const Civil& local, bool clockValid, const char* zone, const ChipSet& chipSet, bool syncTag) {
  c.scope(DrawScope::Status);
  c.font(Font::Body);
  const int16_t zw = zone && *zone ? c.width(zone) : 0;
  const int16_t zoneX = (int16_t)(kContentX1 - zw + 1);
  char date[24];
  ChipSet s = chipSet;
  bool weekday = true, sync = syncTag && clockValid;
  const int16_t syncW = tagWidth(c, str(S_TAG_SYNC));
  auto dateW = [&](bool wd) {
    if (!clockValid) { c.font(Font::Body); return c.width(str(S_HOME_NOT_SET)); }
    fmtDateShort(date, sizeof date, local, wd);
    c.font(Font::Body);
    return c.width(date);
  };
  auto groupW = [&](const ChipSet& cs, bool withSync) {
    int16_t w = chipsWidth(c, cs);
    if (withSync) w = (int16_t)(w + (w ? kChipGap : 0) + syncW);
    return w;
  };
  const int16_t right = (int16_t)(zoneX - 1 - kChipGap);
  auto fits = [&](bool wd, const ChipSet& cs, bool withSync) {
    const int16_t L = (int16_t)(dateW(wd) + kChipGap);
    return right - groupW(cs, withSync) + 1 >= L;
  };
  // §3.4 overflow steps: (1) drop the weekday, (2) fold SYNC? into the warning chip, (3) drop chips (in chips()).
  if (!fits(true, s, sync)) {
    weekday = false;
    if (!fits(false, s, sync) && sync) { sync = false; s.warn = true; }
  }
  const int16_t dw = dateW(weekday);
  c.font(Font::Body);
  c.text(0, 1, clockValid ? date : str(S_HOME_NOT_SET));
  if (zw) c.text(kContentX1, 1, zone, Align::Right);
  const int16_t L = (int16_t)(dw + kChipGap);
  int16_t left = chips(c, right, L, 2, 2, s);
  if (sync) {
    const int16_t x = (int16_t)(left - (left <= right ? kChipGap : 0) - syncW);
    if (x >= L) tag(c, x, 1, str(S_TAG_SYNC));
  }
  c.scope(DrawScope::Content);
}

void battery(Canvas& c, int16_t x, int16_t y, const BatteryState& b, bool blinkOn) {
  const bool low = b.present && b.pct <= 10;
  if (!low || blinkOn) c.frame(Rect{x, y, 12, 7});
  c.box(Rect{(int16_t)(x + 12), (int16_t)(y + 2), 2, 3});
  if (!b.present) {
    for (int i = 0; i < 3; ++i) c.box(Rect{(int16_t)(x + 2 + 3 * i), (int16_t)(y + 3), 2, 1});
    return;
  }
  const int16_t w = (int16_t)lroundf(8.0f * (b.pct > 100 ? 100 : b.pct) / 100.0f);
  if (w > 0) c.box(Rect{(int16_t)(x + 2), (int16_t)(y + 2), w, 3});
}

void progressBar(Canvas& c, int16_t y, float p) {
  if (p < 0) p = 0;
  if (p > 1) p = 1;
  c.frame(Rect{0, y, 117, 5});
  const int16_t w = (int16_t)floorf(115.0f * p);
  if (w > 0) c.box(Rect{1, (int16_t)(y + 1), w, 3});
  static const int16_t kTicks[3] = {29, 58, 87};   // quarter ticks: colour 0 where filled, lit where empty
  for (int16_t tx : kTicks) {
    c.color(tx < 1 + w ? 0 : 1);
    c.pixel(tx, (int16_t)(y + 2));
  }
  c.color(1);
}

void cardHeader(Canvas& c, const char* title) {
  c.scope(DrawScope::Card);
  c.rbox(Rect{0, 0, 117, 12}, 2);
  c.color(0);
  c.icon(3, 3, IconId::SWarn);
  c.font(Font::Label);
  c.text(11, 2, title);
  c.color(1);
}

void toast(Canvas& c, const char* text, int16_t dy) {
  if (!text || !*text) return;
  c.scope(DrawScope::Toast);
  c.edgeClip(dy > 0);
  c.font(Font::Body);
  const int16_t tw = c.width(text);
  const int16_t w = (int16_t)(tw + 10);
  const int16_t x = (int16_t)((117 - w) / 2);
  c.color(0);
  c.box(Rect{(int16_t)(x - 1), (int16_t)(49 + dy), (int16_t)(w + 2), 15});
  c.color(1);
  c.rbox(Rect{x, (int16_t)(50 + dy), w, 13}, 3);
  c.color(0);
  c.text((int16_t)(x + 5), (int16_t)(53 + dy), text);
  c.color(1);
  c.edgeClip(false);
  c.scope(DrawScope::Content);
}

namespace {
void pageDots(Canvas& c, const RailState& r) {
  int16_t y = kDotsTop;
  for (uint8_t i = 0; i < r.pageCount; ++i) {
    float wgt = i == r.page ? 1.0f : 0.0f;
    if (r.slideT < 1.0f && r.fromPage != r.page) {
      if (i == r.page) wgt = r.slideT;
      else if (i == r.fromPage) wgt = 1.0f - r.slideT;
    }
    const int16_t h = (int16_t)lroundf(2.0f + 4.0f * wgt);
    if (wgt > 0.5f) c.box(Rect{kDotsX, y, 3, h});
    else c.box(Rect{(int16_t)(kDotsX + 1), y, 2, h});
    y = (int16_t)(y + h + 2);
  }
}

void keycap(Canvas& c, int16_t capY, const Binding& b, Key k, const RailState& r) {
  const bool holding = r.hold.active && r.hold.key == k;
  const bool pressed = (r.pressedMask & keyBit(k)) != 0;
  const IconId icon = holding && b.holdIcon != IconId::None ? b.holdIcon : b.icon;
  const Rect cap{kRailX, capY, kCapW, kCapH};
  if (holding && r.flash) {
    c.rbox(cap, 1);
    c.color(0);
    c.icon(kCapIconX, (int16_t)(capY + kCapIconDy), icon);
    c.color(1);
    return;
  }
  if (pressed && !holding) {
    c.rbox(cap, 1);
    c.color(0);
    c.icon(kCapIconX, (int16_t)(capY + kCapIconDy), icon);
    c.color(1);
    return;
  }
  c.rframe(cap, 1);
  if (holding) {
    const int16_t h = (int16_t)lroundf(8.0f * r.hold.progress);
    if (h > 0) c.box(Rect{(int16_t)(kRailX + 1), (int16_t)(capY + kCapH - 1 - h), 7, h});
    c.color(2);
    c.icon(kCapIconX, (int16_t)(capY + kCapIconDy), icon);
    c.color(1);
    return;
  }
  c.icon(kCapIconX, (int16_t)(capY + kCapIconDy), icon);
}

void keyLabel(Canvas& c, int16_t capY, const Binding& b, Key k, const RailState& r) {
  if (!b.labelStr || b.labelMode == LabelMode::None) return;
  const bool holding = r.hold.active && r.hold.key == k;
  const char* t = str(b.labelStr);
  c.font(Font::Body);
  const int16_t w = c.width(t);
  if (holding) {   // pill while holding (both modes)
    c.scope(DrawScope::Pill);
    c.color(0);
    c.box(Rect{(int16_t)(kLabelRight - w - 5), capY, (int16_t)(w + 6), 10});
    c.color(1);
    c.rbox(Rect{(int16_t)(kLabelRight - w - 4), capY, (int16_t)(w + 5), 10}, 2);
    c.color(0);
    c.text((int16_t)(kLabelRight - 1), (int16_t)(capY + kLabelInkDy), t, Align::Right);
    c.color(1);
    return;
  }
  if (b.labelMode != LabelMode::Static) return;
  c.scope(DrawScope::Label);
  c.text(kLabelRight, (int16_t)(capY + kLabelInkDy), t, Align::Right);
}
}  // namespace

void rail(Canvas& c, const Bindings& b, const RailState& r) {
  if (b.railHidden || b.captureAll) return;   // the alert draws no rail; Diagnostics > Buttons draws its own boxes
  c.scope(DrawScope::Rail);
  if (b.chevrons) {
    c.icon(kChevX, kChevUpY, IconId::ChevUp);
    c.icon(kChevX, kChevDnY, IconId::ChevDown);
  } else if (r.dots) {
    pageDots(c, r);
  }
  const Key keys[2] = {Key::Hash, Key::Star};
  const int16_t capY[2] = {kCapHashY, kCapStarY};
  for (int i = 0; i < 2; ++i) {
    const Binding& kb = b.key[(int)keys[i]];
    if (kb.icon == IconId::None) continue;            // no icon => no action (test_keymap)
    c.scope(DrawScope::Rail);
    keycap(c, capY[i], kb, keys[i], r);
    keyLabel(c, capY[i], kb, keys[i], r);
  }
  c.scope(DrawScope::Content);
}

}  // namespace widgets
}  // namespace hg
