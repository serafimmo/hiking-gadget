#pragma once
#include "icons.h"
#include "u8g2_inc.h"
namespace hg {
enum class Font : uint8_t { Hero, Big, BigS, Heading, Mid, Label, Value, Body, Micro, Count };
enum class Align : uint8_t { Left, Center, Right };
struct Rect { int16_t x, y, w, h; };
constexpr char    kDegChar = '`';                      // drawn as the small degree primitive (§3.3, D29)
constexpr int16_t kDegAdvance = 4;
// Layout audit hook (host tests set one; the device never does). Every accepted draw is reported with its ink box.
enum class DrawScope : uint8_t { Content, Status, Rail, Label, Pill, Toast, Card, Overlay };
struct DrawOp {
  enum class Kind : uint8_t { Text, Pixel, Line, Box, Frame, Circle, Disc, Triangle, Icon, Degree } kind;
  Rect box;                // ink bounding box (x, y, w, h), clipped to the screen
  Font font;               // Text only
  uint8_t color;
  DrawScope scope;
  const char* text;        // Text only (valid during the callback)
};
struct DrawRecorder { virtual void record(const DrawOp& op) = 0; };
int16_t fontCapHeight(Font f);                         // ink rows of caps/digits (§3.3 table)
int16_t fontDescent(Font f);                           // rows below the baseline (Body 2)
class Canvas {
 public:
  explicit Canvas(u8g2_t* g);
  void clear();  void color(uint8_t c);                // 0 clear, 1 set, 2 XOR (not for circle/disc)
  void clipContent();  void clipNone();                // content = x 0..116
  void font(Font f);
  int16_t width(const char* s) const;                  // u8g2_GetStrWidth over the runs between kDegChar + kDegAdvance each (static text)
  int16_t numWidth(const char* s) const;               // sum of advances, digits counted as '0', kDegChar = kDegAdvance
  int16_t text(int16_t x, int16_t inkTop, const char* s, Align a = Align::Left);   // kDegChar -> small degree
  int16_t num(int16_t x, int16_t inkTop, const char* s, Align a = Align::Left);    // kDegChar -> small degree
  void pixel(int16_t x, int16_t y);  void hline(int16_t x, int16_t y, int16_t w);  void vline(int16_t x, int16_t y, int16_t h);
  void dottedH(int16_t x, int16_t y, int16_t w);  void dottedV(int16_t x, int16_t y, int16_t h);
  void box(Rect r);  void frame(Rect r);  void rbox(Rect r, uint8_t rad);  void rframe(Rect r, uint8_t rad);
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);   // signed Bresenham, per-pixel clip
  void circle(int16_t cx, int16_t cy, uint8_t r);  void disc(int16_t cx, int16_t cy, uint8_t r);  // must fit on screen
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
  void icon(int16_t x, int16_t y, IconId id);
  void degree(int16_t x, int16_t inkTop, bool large);
  void polar(int16_t cx, int16_t cy, float r, float deg, int16_t& x, int16_t& y) const;   // 0 deg = up, clockwise
  uint8_t* buffer();                                   // 1024 B, page-major, LSB = top row
  uint16_t rejected() const;                           // draws refused (negative/off-screen); tests assert 0; on the device
                                                       // UiController logs once per screen per boot when it grows (§4.5 step 8)
  void setRecorder(DrawRecorder* r);
  void scope(DrawScope s);                             // tags the following draws for the layout audit
  // Motion only (toast slide): shapes may cross the right/bottom edge and are clipped instead of rejected.
  // Negative coordinates are still rejected (they would wrap in u8g2's unsigned coordinates).
  void edgeClip(bool on);
  u8g2_t* u8g2();

 private:
  bool accept(int16_t x, int16_t y, int16_t w, int16_t h);   // inside 128x64, else counted as rejected
  void record(DrawOp::Kind k, int16_t x, int16_t y, int16_t w, int16_t h, const char* text = nullptr);
  int16_t drawRuns(int16_t x, int16_t y, const char* s, bool tabular);
  u8g2_t* g_;
  DrawRecorder* rec_ = nullptr;
  Font font_ = Font::Body;
  uint8_t color_ = 1;
  DrawScope scope_ = DrawScope::Content;
  bool edgeClip_ = false;
  uint16_t rejected_ = 0;
};
}  // namespace hg
