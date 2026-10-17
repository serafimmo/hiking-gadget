// ui/canvas on the installed U8g2 (SPEC §3.3, §4.4): ink-top offset of all 9 fonts, tabular widths, rejection, lines.
#include "fakes/host_u8g2.h"
#include "tiny_test.h"
#include "ui/theme.h"

using namespace hg;

namespace {
struct Rec : DrawRecorder {
  int n = 0; DrawOp last{};
  void record(const DrawOp& op) override { ++n; last = op; }
};
int firstLitRow(HostU8g2& h) { for (int y = 0; y < 64; ++y) for (int x = 0; x < 128; ++x) if (h.px(x, y)) return y; return -1; }
int lastLitRow(HostU8g2& h) { for (int y = 63; y >= 0; --y) for (int x = 0; x < 128; ++x) if (h.px(x, y)) return y; return -1; }
}  // namespace

TEST(canvas_ink_top_offset_is_1_for_all_9_fonts) {
  HostU8g2 h;
  Canvas c(&h.g);
  for (int f = 0; f < (int)Font::Count; ++f) {
    c.clear();
    c.font((Font)f);
    const bool digitsOnly = f <= (int)Font::Heading;
    // §3.3 heights are the union over all digits (logisoso30 '3' sits one row lower than '0')
    u8g2_DrawStr(&h.g, 0, 20, digitsOnly ? "0123456789" : "H0123456789");
    CHECK_EQ(firstLitRow(h) - 20, 1);
    // the measured cap height matches the §3.3 table (used by the layout audit)
    CHECK_EQ(lastLitRow(h) - firstLitRow(h) + 1, (int)fontCapHeight((Font)f));
  }
}

TEST(canvas_text_draws_at_ink_top) {
  HostU8g2 h;
  Canvas c(&h.g);
  c.clear();
  c.font(Font::Body);
  CHECK(c.text(0, 1, "SUN 11 OCT") > 0);
  CHECK_EQ(firstLitRow(h), 1);
  c.clear();
  c.font(Font::Hero);
  c.num(0, 11, "13:05");                                    // the 3 reaches the bottom of the font box
  CHECK_EQ(firstLitRow(h), 11);
  CHECK_EQ(lastLitRow(h), 41);                              // §3.8.1: rows 11..41
  CHECK_EQ(c.rejected(), 0);
}

TEST(canvas_num_width_is_stable) {
  HostU8g2 h;
  Canvas c(&h.g);
  for (int f = 0; f < (int)Font::Count; ++f) {
    c.font((Font)f);
    CHECK_EQ(c.numWidth("11:11"), c.numWidth("88:88"));
  }
  c.font(Font::Hero);
  CHECK_EQ(c.numWidth("88:88"), 19 * 4 + 11);              // §3.3: digit 19, ':' 11
  c.font(Font::Body);
  CHECK_EQ(c.numWidth("1`"), c.numWidth("0") + kDegAdvance);
  CHECK(c.width("1") < c.numWidth("1"));                   // Body '1' is narrow: num() pads it
  // right alignment of changing numbers does not jitter
  c.clear();
  c.font(Font::Mid);
  c.num(115, 11, "11", Align::Right);
  int r1 = -1; for (int x = 127; x >= 0 && r1 < 0; --x) for (int y = 0; y < 64; ++y) if (h.px(x, y)) { r1 = x; break; }
  CHECK(r1 <= 115);
}

TEST(canvas_rejects_negative_and_offscreen) {
  HostU8g2 h;
  Canvas c(&h.g);
  c.clear();
  c.box(Rect{-1, 0, 4, 4});
  c.frame(Rect{120, 60, 10, 10});
  c.disc(2, 30, 5);
  c.circle(64, 62, 3);
  c.font(Font::Body);
  c.text(0, 0, "X");                                        // inkTop must be >= 1
  c.text(125, 10, "WIDE");
  c.pixel(128, 0);
  c.icon(126, 0, IconId::Play);
  CHECK_EQ(c.rejected(), 8);
  CHECK_EQ(h.lit(), 0);
  c.box(Rect{0, 0, 128, 64});
  CHECK_EQ(c.rejected(), 8);
  CHECK_EQ(h.lit(), 128 * 64);
}

TEST(canvas_signed_line_clips_per_pixel) {
  HostU8g2 h;
  Canvas c(&h.g);
  c.clear();
  c.line(-10, -10, 10, 10);                                 // only the on-screen half is drawn
  CHECK(h.px(0, 0)); CHECK(h.px(10, 10)); CHECK(!h.px(11, 11));
  CHECK_EQ(h.lit(), 11);
  c.clear();
  c.line(120, 30, 140, 30);
  CHECK_EQ(h.lit(), 8);
  CHECK_EQ(c.rejected(), 0);
  c.clear();
  c.line(-5, -5, -1, -1);                                   // fully off-screen: nothing, no wrap-around garbage
  CHECK_EQ(h.lit(), 0);
}

TEST(canvas_degree_and_recorder) {
  HostU8g2 h;
  Canvas c(&h.g);
  Rec rec;
  c.setRecorder(&rec);
  c.clear();
  c.font(Font::Body);
  c.scope(DrawScope::Toast);
  const int16_t w = c.text(10, 20, "Heading 045`");   // 'g' has a descender
  CHECK(rec.n == 1);
  CHECK_EQ(rec.last.kind, DrawOp::Kind::Text);
  CHECK_EQ(rec.last.scope, DrawScope::Toast);
  CHECK_EQ(rec.last.box.w, w);
  CHECK_EQ(rec.last.box.h, 9);                              // Body: 7 + descender 2
  c.text(10, 40, "HOLD 12");
  CHECK_EQ(rec.last.box.h, 7);                              // no descender: cap height only
  // the small degree diamond at the end
  const int dx = 10 + w - kDegAdvance;
  CHECK(h.px(dx + 1, 20)); CHECK(h.px(dx, 21)); CHECK(h.px(dx + 2, 21)); CHECK(h.px(dx + 1, 22)); CHECK(!h.px(dx + 1, 21));
  c.degree(50, 40, true);
  CHECK(h.px(52, 40)); CHECK(!h.px(52, 42));                // ring r 2 around (52,42)
}

TEST(canvas_icons_have_their_spec_sizes) {
  for (int i = 1; i < (int)IconId::Count; ++i) {
    const IconBitmap& b = iconBitmap((IconId)i);
    CHECK(b.w >= 3 && b.h >= 3);
    int lit = 0;
    for (int k = 0; k < b.w * b.h; ++k) { CHECK(b.rows[k] == '#' || b.rows[k] == '.'); lit += b.rows[k] == '#'; }
    CHECK(lit > 0);
  }
  CHECK_EQ(iconBitmap(IconId::Bell).w, 7);
  CHECK_EQ(iconBitmap(IconId::SWarn).h, 5);
  CHECK_EQ(iconBitmap(IconId::Live).w, 3);
  CHECK_EQ(iconBitmap(IconId::ChevUp).h, 3);
  CHECK_EQ(iconBitmap(IconId::None).w, 0);
}
