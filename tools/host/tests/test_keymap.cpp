// Key-map rules (SPEC §2.4, §10.3 test_keymap) over every fixture and every sweep state.
#include <cstdio>
#include <cstring>

#include "fixtures.h"
#include "tiny_test.h"
#include "ui/strings.h"
#include "ui/theme.h"
#include "ui/ui_controller.h"

using namespace hg;

namespace {
struct Rec : DrawRecorder {
  bool capIcon[2] = {false, false};   // # and *
  bool anyKeyText = false;
  void record(const DrawOp& op) override {
    if (op.kind == DrawOp::Kind::Icon && op.scope == DrawScope::Rail && op.box.x == kCapIconX) {
      if (op.box.y == kCapHashY + kCapIconDy) capIcon[0] = true;
      if (op.box.y == kCapStarY + kCapIconDy) capIcon[1] = true;
    }
    if (op.kind == DrawOp::Kind::Text && op.text && strcmp(op.text, str(S_ANY_KEY_OK)) == 0) anyKeyText = true;
  }
};
int failures = 0;
void fail(const char* name, const char* why) { if (++failures <= 40) printf("  KEYMAP %s: %s\n", name, why); }

void check(const char* name, const FixtureCtx& f) {
  Rec rec;
  RenderOut out{};
  renderFixture(f, out, &rec);
  const Bindings& b = out.bindings;
  const bool carousel = out.top == UiController::pageScreen(f.ui.page) && f.ui.depth == 0;
  if (b.captureAll) return;   // Diagnostics > Buttons: every key captured, exit rule in the controller
  if (b.railHidden) {
    const ActionId a = b.key[0].shortAct;
    bool same = true;
    for (int k = 0; k < kKeyCount; ++k) if (b.key[k].shortAct != a || b.key[k].longAct != ActionId::None) same = false;
    if (!same) fail(name, "railHidden: keys differ or have long actions");
    if (a != ActionId::None && !rec.anyKeyText) fail(name, "railHidden with actions but no 'Any key = OK'");
    return;
  }
  for (int k = 0; k < kKeyCount; ++k) {
    const Binding& kb = b.key[k];
    if (kb.longAct != ActionId::None) {
      if (keyConfigFor(kb).shortTrigger != Trigger::OnClick) fail(name, "short on Down together with a long action");
      if (kb.labelMode == LabelMode::None && kb.holdIcon == IconId::None) fail(name, "hold action not advertised");
      if (kb.shortAct == ActionId::ShowHint && !kb.hintStr) fail(name, "hint action without a hint string");
    }
    if (k >= 2) {
      const bool acts = kb.shortAct != ActionId::None || kb.longAct != ActionId::None;
      if (kb.icon == IconId::None && acts) fail(name, "#/* action without an icon");
      if ((kb.icon != IconId::None) != rec.capIcon[k - 2]) fail(name, "rail keycap does not match the binding");
    } else {
      const bool acts = kb.shortAct != ActionId::None || kb.longAct != ActionId::None;
      if (acts && !carousel && !b.chevrons) fail(name, "^/v act without dots or chevrons");
    }
  }
}
struct Sink : SweepSink { int n = 0; void emit(const char* name, const FixtureCtx& f) override { ++n; check(name, f); } };
}  // namespace

TEST(keymap_rules_every_fixture_and_sweep) {
  failures = 0;
  for (int i = 0; i < fixtureCount(); ++i) {
    FixtureCtx f = baseWorld();
    fixtureAt(i).fn(f);
    check(fixtureAt(i).name, f);
  }
  Sink s;
  for (int i = 0; i < sweepCount(); ++i) runSweep(i, s);
  printf("  keymap: %d fixtures, %d sweep states, %d findings\n", fixtureCount(), s.n, failures);
  CHECK_EQ(failures, 0);
}
