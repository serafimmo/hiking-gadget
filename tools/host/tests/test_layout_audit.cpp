// Layout audit (SPEC §10.3): DrawRecorder over every fixture AND every sweep. Run alone with 'make audit'.
#include <cstdio>
#include <cstring>

#include "fixtures.h"
#include "tiny_test.h"
#include "ui/theme.h"

using namespace hg;

namespace {
struct Audit : DrawRecorder {
  static constexpr int kMax = 400;
  DrawOp ops[kMax];
  char texts[kMax][48];
  int n = 0;
  void record(const DrawOp& op) override {
    if (n >= kMax) return;
    ops[n] = op;
    texts[n][0] = 0;
    if (op.text) { strncpy(texts[n], op.text, sizeof texts[n] - 1); texts[n][sizeof texts[n] - 1] = 0; }
    ++n;
  }
};

bool intersects(const Rect& a, const Rect& b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
bool exemptScope(DrawScope s) { return s == DrawScope::Toast || s == DrawScope::Pill || s == DrawScope::Card || s == DrawScope::Overlay; }

int failures = 0;
void fail(const char* name, const char* why, const char* detail = "") {
  ++failures;
  if (failures <= 60) printf("  AUDIT %s: %s %s\n", name, why, detail);
}

void auditOne(const char* name, const FixtureCtx& f) {
  Audit a;
  RenderOut out{};
  renderFixture(f, out, &a);
  if (out.rejected) { char d[16]; snprintf(d, sizeof d, "(%u)", out.rejected); fail(name, "draws rejected", d); }
  const bool railShown = !out.bindings.railHidden;
  const bool microAllowedScreen = out.top == ScreenId::Diagnostics || out.top == ScreenId::SettingsList;
  for (int i = 0; i < a.n; ++i) {
    const DrawOp& o = a.ops[i];
    const int16_t x1 = (int16_t)(o.box.x + o.box.w - 1);
    if (o.box.x < 0 || o.box.y < 0 || x1 > 127 || o.box.y + o.box.h > 64) fail(name, "outside 128x64", a.texts[i]);
    const bool content = o.scope == DrawScope::Content || o.scope == DrawScope::Status;
    if (railShown && content && x1 > kContentX1) fail(name, "content right of x 116", a.texts[i]);
    if (o.scope == DrawScope::Rail && o.box.x < kRailX) fail(name, "rail op left of x 119", a.texts[i]);
    if (o.kind == DrawOp::Kind::Text) {
      if (o.font == Font::Micro || o.font == Font::Label)
        for (const char* p = a.texts[i]; *p; ++p)
          if (*p >= 'a' && *p <= 'z') { fail(name, "lowercase in a caps font", a.texts[i]); break; }
      if (o.font == Font::Micro && o.scope == DrawScope::Content && o.color != 0 && !microAllowedScreen)
        fail(name, "Micro text outside status line / tags / Diagnostics / list headers", a.texts[i]);
      for (int j = i + 1; j < a.n; ++j) {
        const DrawOp& p = a.ops[j];
        if (p.kind != DrawOp::Kind::Text || exemptScope(o.scope) || exemptScope(p.scope)) continue;
        if (intersects(o.box, p.box)) {
          char d[100];
          snprintf(d, sizeof d, "\"%s\" x \"%s\"", a.texts[i], a.texts[j]);
          fail(name, "text boxes overlap", d);
        }
      }
    }
  }
  // the static keycap label box must be free of other content ink
  for (int i = 0; i < a.n; ++i) {
    if (a.ops[i].scope != DrawScope::Label) continue;
    const Rect lb{(int16_t)(a.ops[i].box.x - 1), (int16_t)(a.ops[i].box.y - 2), (int16_t)(a.ops[i].box.w + 1), 10};
    for (int j = 0; j < a.n; ++j)
      if ((a.ops[j].scope == DrawScope::Content || a.ops[j].scope == DrawScope::Status) && intersects(lb, a.ops[j].box))
        fail(name, "content inside the keycap label box", a.texts[j]);
  }
}

struct AuditSink : SweepSink {
  int n = 0;
  void emit(const char* name, const FixtureCtx& f) override { ++n; auditOne(name, f); }
};
}  // namespace

TEST(layout_audit_fixtures_and_sweeps) {
  failures = 0;
  for (int i = 0; i < fixtureCount(); ++i) {
    const FixtureDef& d = fixtureAt(i);
    FixtureCtx f = baseWorld();
    d.fn(f);
    auditOne(d.name, f);
  }
  AuditSink sink;
  for (int i = 0; i < sweepCount(); ++i) runSweep(i, sink);
  printf("  layout audit: %d fixtures, %d sweep states, %d findings\n", fixtureCount(), sink.n, failures);
  CHECK_EQ(failures, 0);
}
