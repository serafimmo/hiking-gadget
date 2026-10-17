#include "canvas.h"

#include <math.h>
#include <string.h>

#include "theme.h"

namespace hg {
namespace {
const uint8_t* fontData(Font f) {
  switch (f) {
    case Font::Hero:    return u8g2_font_logisoso30_tn;
    case Font::Big:     return u8g2_font_logisoso28_tn;
    case Font::BigS:    return u8g2_font_logisoso24_tn;
    case Font::Heading: return u8g2_font_logisoso22_tn;
    case Font::Mid:     return u8g2_font_logisoso16_tr;
    case Font::Label:   return u8g2_font_helvB08_tr;
    case Font::Value:   return u8g2_font_helvB10_tr;
    case Font::Micro:   return u8g2_font_tom_thumb_4x6_mr;
    default:            return u8g2_font_haxrcorp4089_tr;
  }
}
// §3.3: ink height of caps/digits; descent below the baseline (only Body/Label/Value/Micro have lowercase or
// descending glyphs in use).
constexpr int16_t kFontCapH[(int)Font::Count] = {31, 28, 24, 23, 16, 8, 11, 7, 5};
constexpr int16_t kDescent[(int)Font::Count] = {0, 0, 0, 0, 3, 2, 3, 2, 1};
constexpr int16_t kInkTopOffset = 1;   // every font puts ink 1 row below the FontPosTop origin (test_canvas measures it)
bool isDigit(char c) { return c >= '0' && c <= '9'; }
// Glyphs that reach below the baseline in our fonts; a text box includes the font descent only if it has one.
bool hasDescender(const char* s) {
  for (; *s; ++s)
    if (*s && strchr("gjpqy,;()[]{}|_@$", *s)) return true;
  return false;
}
}  // namespace

int16_t fontCapHeight(Font f) { return kFontCapH[(int)f < (int)Font::Count ? (int)f : 0]; }
int16_t fontDescent(Font f) { return kDescent[(int)f < (int)Font::Count ? (int)f : 0]; }

Canvas::Canvas(u8g2_t* g) : g_(g) {
  u8g2_SetFontMode(g_, 1);
  u8g2_SetFontPosTop(g_);
  font(Font::Body);
}

void Canvas::clear() { u8g2_ClearBuffer(g_); color(1); clipNone(); scope_ = DrawScope::Content; edgeClip_ = false; }
void Canvas::color(uint8_t c) { color_ = c; u8g2_SetDrawColor(g_, c); }
void Canvas::clipContent() { u8g2_SetClipWindow(g_, 0, 0, kContentX1 + 1, kH); }
void Canvas::clipNone() { u8g2_SetMaxClipWindow(g_); }
void Canvas::font(Font f) {
  font_ = f;
  u8g2_SetFont(g_, fontData(f));
  u8g2_SetFontMode(g_, 1);
  u8g2_SetFontPosTop(g_);
}
void Canvas::setRecorder(DrawRecorder* r) { rec_ = r; }
void Canvas::scope(DrawScope s) { scope_ = s; }
u8g2_t* Canvas::u8g2() { return g_; }
uint8_t* Canvas::buffer() { return u8g2_GetBufferPtr(g_); }
uint16_t Canvas::rejected() const { return rejected_; }

void Canvas::edgeClip(bool on) { edgeClip_ = on; }

bool Canvas::accept(int16_t x, int16_t y, int16_t w, int16_t h) {
  if (edgeClip_ && x >= 0 && y >= 0 && (x >= kW || y >= kH)) return false;   // motion: fully past the edge = invisible
  if (x < 0 || y < 0 || w < 0 || h < 0 || (!edgeClip_ && (x + w > kW || y + h > kH)) || x >= kW || y >= kH) {
    ++rejected_;
    return false;
  }
  return true;
}

void Canvas::record(DrawOp::Kind k, int16_t x, int16_t y, int16_t w, int16_t h, const char* text) {
  if (!rec_) return;
  DrawOp op{k, Rect{x, y, w, h}, font_, color_, scope_, text};
  rec_->record(op);
}

// Static text width: glyph advances up to each kDegChar (where drawing continues), u8g2_GetStrWidth (ink extent)
// for the last run, kDegAdvance per kDegChar.
int16_t Canvas::width(const char* s) const {
  if (!s) return 0;
  int16_t w = 0;
  const char* run = s;
  for (const char* p = s;; ++p) {
    if (*p != kDegChar && *p != 0) continue;
    if (*p == kDegChar) {
      for (const char* q = run; q < p; ++q) w = (int16_t)(w + u8g2_GetGlyphWidth(g_, (uint8_t)*q));
      w = (int16_t)(w + kDegAdvance);
      run = p + 1;
      continue;
    }
    if (p > run) {
      char buf[64];
      size_t n = (size_t)(p - run) < sizeof buf - 1 ? (size_t)(p - run) : sizeof buf - 1;
      for (size_t i = 0; i < n; ++i) buf[i] = run[i];
      buf[n] = 0;
      w = (int16_t)(w + u8g2_GetStrWidth(g_, buf));
    }
    break;
  }
  return w;
}

int16_t Canvas::numWidth(const char* s) const {
  if (!s) return 0;
  const int16_t zero = u8g2_GetGlyphWidth(g_, '0');
  int16_t w = 0;
  for (const char* p = s; *p; ++p) {
    if (*p == kDegChar) w = (int16_t)(w + kDegAdvance);
    else if (isDigit(*p)) w = (int16_t)(w + zero);
    else w = (int16_t)(w + u8g2_GetGlyphWidth(g_, (uint8_t)*p));
  }
  return w;
}

// Draws s at (x, y = inkTop - 1); kDegChar becomes the small degree. tabular: digits centred in '0' cells.
int16_t Canvas::drawRuns(int16_t x, int16_t y, const char* s, bool tabular) {
  const int16_t zero = tabular ? u8g2_GetGlyphWidth(g_, '0') : 0;
  int16_t cx = x;
  for (const char* p = s; *p; ++p) {
    if (*p == kDegChar) {
      const int16_t t = (int16_t)(y + kInkTopOffset);
      u8g2_DrawPixel(g_, (u8g2_uint_t)(cx + 1), (u8g2_uint_t)t);
      u8g2_DrawPixel(g_, (u8g2_uint_t)cx, (u8g2_uint_t)(t + 1));
      u8g2_DrawPixel(g_, (u8g2_uint_t)(cx + 2), (u8g2_uint_t)(t + 1));
      u8g2_DrawPixel(g_, (u8g2_uint_t)(cx + 1), (u8g2_uint_t)(t + 2));
      cx = (int16_t)(cx + kDegAdvance);
      continue;
    }
    const char one[2] = {*p, 0};
    if (tabular && isDigit(*p)) {
      const int16_t adv = u8g2_GetGlyphWidth(g_, (uint8_t)*p);
      u8g2_DrawStr(g_, (u8g2_uint_t)(cx + (zero - adv) / 2), (u8g2_uint_t)y, one);
      cx = (int16_t)(cx + zero);
    } else {
      cx = (int16_t)(cx + u8g2_DrawStr(g_, (u8g2_uint_t)cx, (u8g2_uint_t)y, one));
    }
  }
  return (int16_t)(cx - x);
}

int16_t Canvas::text(int16_t x, int16_t inkTop, const char* s, Align a) {
  if (!s || !*s) return 0;
  const int16_t w = width(s);
  int16_t x0 = a == Align::Left ? x : a == Align::Center ? (int16_t)(x - w / 2) : (int16_t)(x - w + 1);
  const int16_t h = (int16_t)(fontCapHeight(font_) + (hasDescender(s) ? fontDescent(font_) : 0));
  if (inkTop < 1) { ++rejected_; return 0; }
  if (!accept(x0, inkTop, w, h)) return 0;
  drawRuns(x0, (int16_t)(inkTop - kInkTopOffset), s, false);
  record(DrawOp::Kind::Text, x0, inkTop, w, h, s);
  return w;
}

int16_t Canvas::num(int16_t x, int16_t inkTop, const char* s, Align a) {
  if (!s || !*s) return 0;
  const int16_t w = numWidth(s);
  int16_t x0 = a == Align::Left ? x : a == Align::Center ? (int16_t)(x - w / 2) : (int16_t)(x - w + 1);
  const int16_t h = (int16_t)(fontCapHeight(font_) + (hasDescender(s) ? fontDescent(font_) : 0));
  if (inkTop < 1) { ++rejected_; return 0; }
  if (!accept(x0, inkTop, w, h)) return 0;
  drawRuns(x0, (int16_t)(inkTop - kInkTopOffset), s, true);
  record(DrawOp::Kind::Text, x0, inkTop, w, h, s);
  return w;
}

void Canvas::pixel(int16_t x, int16_t y) {
  if (!accept(x, y, 1, 1)) return;
  u8g2_DrawPixel(g_, (u8g2_uint_t)x, (u8g2_uint_t)y);
  record(DrawOp::Kind::Pixel, x, y, 1, 1);
}

void Canvas::hline(int16_t x, int16_t y, int16_t w) {
  if (w <= 0 || !accept(x, y, w, 1)) return;
  u8g2_DrawHLine(g_, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)w);
  record(DrawOp::Kind::Box, x, y, w, 1);
}

void Canvas::vline(int16_t x, int16_t y, int16_t h) {
  if (h <= 0 || !accept(x, y, 1, h)) return;
  u8g2_DrawVLine(g_, (u8g2_uint_t)x, (u8g2_uint_t)y, (u8g2_uint_t)h);
  record(DrawOp::Kind::Box, x, y, 1, h);
}

void Canvas::dottedH(int16_t x, int16_t y, int16_t w) {
  if (w <= 0 || !accept(x, y, w, 1)) return;
  for (int16_t i = 0; i < w; i = (int16_t)(i + 2)) u8g2_DrawPixel(g_, (u8g2_uint_t)(x + i), (u8g2_uint_t)y);
  record(DrawOp::Kind::Line, x, y, w, 1);
}

void Canvas::dottedV(int16_t x, int16_t y, int16_t h) {
  if (h <= 0 || !accept(x, y, 1, h)) return;
  for (int16_t i = 0; i < h; i = (int16_t)(i + 2)) u8g2_DrawPixel(g_, (u8g2_uint_t)x, (u8g2_uint_t)(y + i));
  record(DrawOp::Kind::Line, x, y, 1, h);
}

void Canvas::box(Rect r) {
  if (r.w <= 0 || r.h <= 0 || !accept(r.x, r.y, r.w, r.h)) return;
  u8g2_DrawBox(g_, (u8g2_uint_t)r.x, (u8g2_uint_t)r.y, (u8g2_uint_t)r.w, (u8g2_uint_t)r.h);
  record(DrawOp::Kind::Box, r.x, r.y, r.w, r.h);
}

void Canvas::frame(Rect r) {
  if (r.w <= 0 || r.h <= 0 || !accept(r.x, r.y, r.w, r.h)) return;
  u8g2_DrawFrame(g_, (u8g2_uint_t)r.x, (u8g2_uint_t)r.y, (u8g2_uint_t)r.w, (u8g2_uint_t)r.h);
  record(DrawOp::Kind::Frame, r.x, r.y, r.w, r.h);
}

void Canvas::rbox(Rect r, uint8_t rad) {
  if (r.w <= 0 || r.h <= 0 || !accept(r.x, r.y, r.w, r.h)) return;
  u8g2_DrawRBox(g_, (u8g2_uint_t)r.x, (u8g2_uint_t)r.y, (u8g2_uint_t)r.w, (u8g2_uint_t)r.h, rad);
  record(DrawOp::Kind::Box, r.x, r.y, r.w, r.h);
}

void Canvas::rframe(Rect r, uint8_t rad) {
  if (r.w <= 0 || r.h <= 0 || !accept(r.x, r.y, r.w, r.h)) return;
  u8g2_DrawRFrame(g_, (u8g2_uint_t)r.x, (u8g2_uint_t)r.y, (u8g2_uint_t)r.w, (u8g2_uint_t)r.h, rad);
  record(DrawOp::Kind::Frame, r.x, r.y, r.w, r.h);
}

// Signed Bresenham with a per-pixel clip: endpoints may lie off-screen, nothing wraps (u8g2 coordinates are
// unsigned, so u8g2_DrawLine with a negative end draws garbage [HWF u8g2 U27]).
void Canvas::line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  const int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  const int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, x = x0, y = y0;
  int16_t bx0 = 127, by0 = 63, bx1 = 0, by1 = 0;
  bool any = false;
  for (;;) {
    if (x >= 0 && y >= 0 && x < kW && y < kH) {
      u8g2_DrawPixel(g_, (u8g2_uint_t)x, (u8g2_uint_t)y);
      any = true;
      if (x < bx0) bx0 = (int16_t)x;
      if (y < by0) by0 = (int16_t)y;
      if (x > bx1) bx1 = (int16_t)x;
      if (y > by1) by1 = (int16_t)y;
    }
    if (x == x1 && y == y1) break;
    const int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x += sx; }
    if (e2 <= dx) { err += dx; y += sy; }
  }
  if (any) record(DrawOp::Kind::Line, bx0, by0, (int16_t)(bx1 - bx0 + 1), (int16_t)(by1 - by0 + 1));
}

void Canvas::circle(int16_t cx, int16_t cy, uint8_t r) {
  if (!accept((int16_t)(cx - r), (int16_t)(cy - r), (int16_t)(2 * r + 1), (int16_t)(2 * r + 1))) return;
  u8g2_DrawCircle(g_, (u8g2_uint_t)cx, (u8g2_uint_t)cy, r, U8G2_DRAW_ALL);
  record(DrawOp::Kind::Circle, (int16_t)(cx - r), (int16_t)(cy - r), (int16_t)(2 * r + 1), (int16_t)(2 * r + 1));
}

void Canvas::disc(int16_t cx, int16_t cy, uint8_t r) {
  if (!accept((int16_t)(cx - r), (int16_t)(cy - r), (int16_t)(2 * r + 1), (int16_t)(2 * r + 1))) return;
  u8g2_DrawDisc(g_, (u8g2_uint_t)cx, (u8g2_uint_t)cy, r, U8G2_DRAW_ALL);
  record(DrawOp::Kind::Disc, (int16_t)(cx - r), (int16_t)(cy - r), (int16_t)(2 * r + 1), (int16_t)(2 * r + 1));
}

void Canvas::triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
  const int16_t lx = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
  const int16_t hx = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
  const int16_t ly = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  const int16_t hy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  if (!accept(lx, ly, (int16_t)(hx - lx + 1), (int16_t)(hy - ly + 1))) return;
  u8g2_DrawTriangle(g_, x0, y0, x1, y1, x2, y2);
  record(DrawOp::Kind::Triangle, lx, ly, (int16_t)(hx - lx + 1), (int16_t)(hy - ly + 1));
}

void Canvas::icon(int16_t x, int16_t y, IconId id) {
  const IconBitmap& b = iconBitmap(id);
  if (!b.w || !accept(x, y, b.w, b.h)) return;
  for (uint8_t r = 0; r < b.h; ++r)
    for (uint8_t c = 0; c < b.w; ++c)
      if (b.rows[r * b.w + c] == '#') u8g2_DrawPixel(g_, (u8g2_uint_t)(x + c), (u8g2_uint_t)(y + r));
  record(DrawOp::Kind::Icon, x, y, b.w, b.h);
}

void Canvas::degree(int16_t x, int16_t inkTop, bool large) {
  if (large) {
    if (!accept(x, inkTop, 5, 5)) return;
    u8g2_DrawCircle(g_, (u8g2_uint_t)(x + 2), (u8g2_uint_t)(inkTop + 2), 2, U8G2_DRAW_ALL);
    record(DrawOp::Kind::Degree, x, inkTop, 5, 5);
    return;
  }
  if (!accept(x, inkTop, 3, 3)) return;
  u8g2_DrawPixel(g_, (u8g2_uint_t)(x + 1), (u8g2_uint_t)inkTop);
  u8g2_DrawPixel(g_, (u8g2_uint_t)x, (u8g2_uint_t)(inkTop + 1));
  u8g2_DrawPixel(g_, (u8g2_uint_t)(x + 2), (u8g2_uint_t)(inkTop + 1));
  u8g2_DrawPixel(g_, (u8g2_uint_t)(x + 1), (u8g2_uint_t)(inkTop + 2));
  record(DrawOp::Kind::Degree, x, inkTop, 3, 3);
}

void Canvas::polar(int16_t cx, int16_t cy, float r, float deg, int16_t& x, int16_t& y) const {
  const float a = deg * 0.017453292519943295f;
  x = (int16_t)lroundf((float)cx + r * sinf(a));
  y = (int16_t)lroundf((float)cy - r * cosf(a));
}

}  // namespace hg
