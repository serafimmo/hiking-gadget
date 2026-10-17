#pragma once
// Render scenarios (SPEC §10.2): name = ViewModel + UiState + now. Every derived field (weekday, local time, DST,
// "ends at") is computed from the fixture UTC, never typed by hand.
#include "core/settings.h"
#include "ui/canvas.h"
#include "ui/view_model.h"

namespace hg {
struct FixtureCtx {
  ViewModel vm{};
  UiState ui{};
  MonoUs now{1000LL * kUsPerS};
  Settings settings{};
  ScreenId overlay = ScreenId::Count;     // PowerOff / Splash
  ErrCode cards[4] = {};                  // queued cards after ui.card
  uint8_t cardArgs[4] = {};
  uint8_t nCards = 0;
};
// The default world: Sunday 2026-10-11 19:42:07 MSK, synced clock, SHT4x 23.4 C / 56 %, QMC5883L calibrated,
// heading 45, everything idle, carousel page 0, display on.
FixtureCtx baseWorld();
void setClock(FixtureCtx& f, int64_t utcS, const char* tzId);   // fills vm.clock, vm.fmt.utcS and settings TimeZone
void setTemp(FixtureCtx& f, float tC, float rh);
void showPage(FixtureCtx& f, uint8_t page);
void pushModal(FixtureCtx& f, ScreenId s);
void setToast(FixtureCtx& f, uint16_t strId, const char* arg);   // fully shown
void refreshBg(FixtureCtx& f);                                   // recomputes vm.bg from the models

using FixtureFn = void (*)(FixtureCtx&);
struct FixtureDef { const char* name; FixtureFn fn; int group; int seq; };
struct FixtureReg { FixtureReg(const char* name, FixtureFn fn, int group); };
struct SweepSink { virtual void emit(const char* name, const FixtureCtx& f) = 0; };
using SweepFn = void (*)(SweepSink&);
struct SweepReg { SweepReg(const char* name, SweepFn fn); };

int fixtureCount();
const FixtureDef& fixtureAt(int i);               // sorted by group, then registration order
int sweepCount();
const char* sweepName(int i);
void runSweep(int i, SweepSink& sink);

struct RenderOut { uint8_t buf[1024]; uint16_t rejected; Bindings bindings; ScreenId top; char toast[48]; };
void renderFixture(const FixtureCtx& f, RenderOut& out, DrawRecorder* rec = nullptr);
}  // namespace hg

// Groups order the contact sheet: 1 Home, 2 Stopwatch, 3 Timer, 4 Compass, 5 Settings, 6 Calibration,
// 7 Diagnostics, 8 Cards/system, 9 Motion/toasts.
#define HG_FIXTURE(group, name)                                     \
  static void fx_##name(::hg::FixtureCtx& f);                       \
  static ::hg::FixtureReg fxr_##name(#name, fx_##name, group);      \
  static void fx_##name(::hg::FixtureCtx& f)
#define HG_SWEEP(name)                                              \
  static void sw_##name(::hg::SweepSink& out);                      \
  static ::hg::SweepReg swr_##name(#name, sw_##name);               \
  static void sw_##name(::hg::SweepSink& out)
