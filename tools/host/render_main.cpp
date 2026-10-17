// hg_render: every fixture -> <out>/NN_name.pbm (SPEC §10.1). Never writes into docs/.
//   hg_render <outdir>        renders all PNG fixtures
//   hg_render --list          prints the fixture names
//   hg_render --bindings      prints docs/BUTTONS.md (key table per screen state)
//   hg_render --bindings --lang ru   prints docs/BUTTONS_RU.md
#include <cstdio>
#include <cstring>

#include "bindings_doc.h"
#include "fixtures.h"

using namespace hg;

namespace {
bool writePbm(const char* path, const uint8_t* buf) {
  FILE* fp = fopen(path, "wb");
  if (!fp) return false;
  fprintf(fp, "P4\n128 64\n");
  for (int y = 0; y < 64; ++y)
    for (int xb = 0; xb < 16; ++xb) {
      uint8_t byte = 0;
      for (int b = 0; b < 8; ++b) {
        const int x = xb * 8 + b;
        if ((buf[(y / 8) * 128 + x] >> (y % 8)) & 1) byte |= (uint8_t)(0x80 >> b);   // 1 = lit pixel
      }
      fputc(byte, fp);
    }
  return fclose(fp) == 0;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc > 1 && strcmp(argv[1], "--list") == 0) {
    for (int i = 0; i < fixtureCount(); ++i) printf("%s\n", fixtureAt(i).name);
    return 0;
  }
  if (argc > 1 && strcmp(argv[1], "--bindings") == 0) {
    const bool ru = argc > 3 && strcmp(argv[2], "--lang") == 0 && strcmp(argv[3], "ru") == 0;
    if (argc > 2 && !ru) { fprintf(stderr, "usage: hg_render --bindings [--lang ru]\n"); return 2; }
    const int missing = writeBindingsDoc(stdout, ru ? DocLang::Ru : DocLang::En);
    if (missing) fprintf(stderr, "FAIL --bindings --lang ru: %d state(s) without a Russian description\n", missing);
    return missing ? 1 : 0;
  }
  if (argc < 2) { fprintf(stderr, "usage: hg_render <outdir> | --list | --bindings [--lang ru]\n"); return 2; }
  int bad = 0;
  for (int i = 0; i < fixtureCount(); ++i) {
    const FixtureDef& d = fixtureAt(i);
    FixtureCtx f = baseWorld();
    d.fn(f);
    RenderOut out{};
    renderFixture(f, out);
    char path[512];
    snprintf(path, sizeof path, "%s/%02d_%s.pbm", argv[1], i + 1, d.name);
    if (!writePbm(path, out.buf)) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    if (out.rejected) { fprintf(stderr, "FAIL %s: %u draws rejected\n", d.name, out.rejected); ++bad; }
  }
  printf("hg_render: %d fixtures -> %s%s\n", fixtureCount(), argv[1], bad ? " (with rejected draws)" : "");
  return bad ? 1 : 0;
}
