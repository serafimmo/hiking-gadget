// Every UI string: printable ASCII, glyph coverage in the font that draws it, role width limit with the worst-case
// argument (SPEC §4.10, D29). Also the time-zone table texts.
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "core/tz.h"
#include "fakes/host_u8g2.h"
#include "tiny_test.h"
#include "ui/strings.h"

using namespace hg;

namespace {
struct RoleSpec { Font font; int16_t maxW; };
RoleSpec roleSpec(StrRole r) {
  switch (r) {
    case StrRole::Toast:     return {Font::Body, 105};
    case StrRole::CardTitle: return {Font::Label, 105};
    case StrRole::CardRow:   return {Font::Body, 108};
    case StrRole::Row:       return {Font::Body, 116};
    case StrRole::Label:     return {Font::Label, 116};
    case StrRole::Micro:     return {Font::Micro, 116};
    default:                 return {Font::Body, 0};   // Free: coverage only
  }
}
bool printable(const char* s) { for (; *s; ++s) if ((unsigned char)*s < 0x20 || (unsigned char)*s > 0x7E) return false; return true; }
int uncovered(HostU8g2& h, Canvas& c, Font f, const char* s) {
  c.font(f);
  int n = 0;
  for (; *s; ++s) if (*s != kDegChar && !u8g2_IsGlyph(&h.g, (uint8_t)*s)) ++n;
  return n;
}
}  // namespace

TEST(strings_ascii_coverage_and_width) {
  HostU8g2 h;
  Canvas c(&h.g);
  int badAscii = 0, badGlyph = 0, tooWide = 0, upperBad = 0, badFmt = 0;
  for (uint16_t id = 1; id < S_COUNT; ++id) {
    const char* t = str(id);
    const char* worst = strWorstArg(id);
    char full[160];
    // Only a string with %s is a printf template; "%RH" or "90%" are literal text and must not be formatted.
    if (strstr(t, "%s")) {
      for (const char* p = t; (p = strchr(p, '%')) != nullptr; p += 2)
        if (p[1] != 's' && p[1] != '%') { ++badFmt; printf("  stray '%%' in a template: \"%s\"\n", t); break; }
      snprintf(full, sizeof full, t, worst);
    } else {
      snprintf(full, sizeof full, "%s", t);
    }
    if (!printable(t) || !printable(worst)) { ++badAscii; printf("  non-ASCII: id %u\n", id); }
    const RoleSpec rs = roleSpec(strRole(id));
    if (uncovered(h, c, rs.font, full)) { ++badGlyph; printf("  glyph missing: \"%s\"\n", full); }
    c.font(rs.font);
    if (rs.maxW && c.width(full) > rs.maxW) { ++tooWide; printf("  too wide (%d > %d): \"%s\"\n", c.width(full), rs.maxW, full); }
    if (rs.font == Font::Micro || rs.font == Font::Label)
      for (const char* p = full; *p; ++p) if (*p >= 'a' && *p <= 'z') { ++upperBad; printf("  lowercase in caps role: \"%s\"\n", full); break; }
  }
  CHECK_EQ(badAscii, 0);
  CHECK_EQ(badGlyph, 0);
  CHECK_EQ(tooWide, 0);
  CHECK_EQ(upperBad, 0);
  CHECK_EQ(badFmt, 0);
  CHECK_STR(str(S_NONE), "");
}

TEST(strings_tz_table_ascii_and_fit) {
  HostU8g2 h;
  Canvas c(&h.g);
  for (int i = 0; i < kTzCount; ++i) {
    const TzDef& z = kTzTable[i];
    for (const char* s : {z.id, z.stdAbbr, z.dstAbbr, z.title, z.note}) {
      CHECK(printable(s));
      CHECK_EQ(uncovered(h, c, Font::Body, s), 0);
      CHECK_EQ(uncovered(h, c, Font::Value, s), 0);
    }
    c.font(Font::Body);
    CHECK(c.width(z.stdAbbr) <= 25);                         // widest Home zone label is OMST/SAMT = 25 px
    CHECK(c.width(z.dstAbbr) <= 25);
  }
}

TEST(strings_error_texts_complete) {
  for (int e = 0; e < (int)ErrCode::Count; ++e) {
    CHECK(errTitle((ErrCode)e) > 0);
    CHECK(errHint((ErrCode)e) > 0);
    CHECK_EQ(strRole(errTitle((ErrCode)e)), StrRole::CardRow);
  }
}
