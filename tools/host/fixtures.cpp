#include "fixtures.h"

#include <cstring>

#include "core/env_math.h"
#include "core/tz.h"
#include "fakes/fake_display.h"
#include "fakes/host_u8g2.h"
#include "ui/ui_controller.h"

namespace hg {
namespace {
constexpr int kMaxFixtures = 256, kMaxSweeps = 64;
FixtureDef& slot(int i) { static FixtureDef defs[kMaxFixtures]; return defs[i]; }
int& count() { static int n = 0; return n; }
bool& sorted() { static bool s = false; return s; }
struct SweepDef { const char* name; SweepFn fn; };
SweepDef& sweepSlot(int i) { static SweepDef defs[kMaxSweeps]; return defs[i]; }
int& sweepN() { static int n = 0; return n; }
}  // namespace

FixtureReg::FixtureReg(const char* name, FixtureFn fn, int group) {
  if (count() < kMaxFixtures) { slot(count()) = FixtureDef{name, fn, group, count()}; ++count(); }
  sorted() = false;
}

SweepReg::SweepReg(const char* name, SweepFn fn) {
  if (sweepN() < kMaxSweeps) sweepSlot(sweepN()++) = SweepDef{name, fn};
}

int fixtureCount() { return count(); }

const FixtureDef& fixtureAt(int i) {
  if (!sorted()) {   // stable insertion sort by group (registration order inside a file is preserved)
    for (int a = 1; a < count(); ++a)
      for (int b = a; b > 0 && (slot(b - 1).group > slot(b).group ||
                                (slot(b - 1).group == slot(b).group && slot(b - 1).seq > slot(b).seq)); --b) {
        FixtureDef t = slot(b); slot(b) = slot(b - 1); slot(b - 1) = t;
      }
    sorted() = true;
  }
  return slot(i);
}

int sweepCount() { return sweepN(); }
const char* sweepName(int i) { return sweepSlot(i).name; }
void runSweep(int i, SweepSink& sink) { sweepSlot(i).fn(sink); }

void setClock(FixtureCtx& f, int64_t utcS, const char* tzId) {
  int z = findTz(tzId);
  if (z < 0) z = kTzDefault;
  const TzDef& tz = kTzTable[z];
  ClockView& c = f.vm.clock;
  c.valid = true;
  c.utcS = utcS;
  c.local = utcToLocal(utcS, tz);
  strncpy(c.zone, tzAbbr(tz, utcS), sizeof c.zone - 1);
  c.zone[sizeof c.zone - 1] = 0;
  c.offsetMin = utcOffsetMin(tz, utcS);
  f.vm.fmt.utcS = utcS;
  f.settings.set(SettingId::TimeZone, (int16_t)z);
}

void setTemp(FixtureCtx& f, float tC, float rh) {
  ThermoView& t = f.vm.thermo;
  t.valid = true; t.stale = false; t.tC = tC; t.rh = rh;
  t.dewC = dewPointC(tC, rh);
  t.ahGm3 = absHumidity(tC, rh);
  f.vm.fmt.liveTempC = tC;
  f.vm.fmt.haveTemp = true;
}

void showPage(FixtureCtx& f, uint8_t page) { f.ui.page = page; f.ui.depth = 0; refreshBg(f); }

void pushModal(FixtureCtx& f, ScreenId s) { if (f.ui.depth < 2) f.ui.modal[f.ui.depth++] = s; }

void setToast(FixtureCtx& f, uint16_t strId, const char* arg) {
  f.ui.toastHead = 0;
  f.ui.toastCount = 1;
  f.ui.toastQ[0] = Toast{strId, {}};
  if (arg) strncpy(f.ui.toastQ[0].arg, arg, sizeof f.ui.toastQ[0].arg - 1);
  f.ui.toastShownMs = 500;
}

void refreshBg(FixtureCtx& f) {
  BgChips& b = f.vm.bg;
  b.sw = f.vm.sw.st == Stopwatch::State::Running;
  b.tmr = f.vm.tm.st == Countdown::State::Running;
  b.tmrRemainS = (int32_t)Countdown::displaySeconds(f.vm.tm.remainingUs);
  b.bell = f.vm.tm.missed;
  b.syncTag = f.vm.clock.valid && f.vm.clock.q == ClockQuality::Approx;
}

FixtureCtx baseWorld() {
  FixtureCtx f;
  f.settings = defaultSettings();
  f.vm.settings = nullptr;   // set by renderFixture to the fixture's own copy
  setClock(f, 1791736927, "MSK");
  f.vm.clock.q = ClockQuality::Synced;
  f.vm.clock.src = ClockSource::Serial;
  f.vm.clock.syncAgeS = 2 * 3600;
  ThermoView& t = f.vm.thermo;
  t.h = Health::Ok; t.chip = ShtChip::Sht4x; t.addr = 0x44; t.serial = 0x0F3A21C4;
  setTemp(f, 23.4f, 56.0f);
  t.err = ErrCode::None;
  MagView& m = f.vm.mag;
  m.h = Health::Ok; m.chip = MagChip::Qmc5883l; m.addr = 0x0D; m.idByte = 0xFF; m.valid = true; m.calibrated = true;
  m.headingDeg = 45.2f; m.shownDeg = 45; m.cardDeg = 45.2f; m.fieldUt = 16.6f; m.rateHz = 50; m.drdy = DrdyKind::RisingLevel;
  m.raw[0] = -1234; m.raw[1] = 2051; m.raw[2] = -5810;
  m.err = ErrCode::None;
  f.vm.fmt.sht = ShtChip::Sht4x;
  f.vm.fmt.mag = MagChip::Qmc5883l;
  f.vm.fmt.liveHeadingDeg = 45.2f;
  f.vm.fmt.haveHeading = true;
  f.vm.fmt.calPct = 94;
  f.vm.fmt.calGrade = 0;
  f.vm.sw.st = Stopwatch::State::Idle;
  f.vm.tm.st = Countdown::State::Select;
  f.vm.tm.preset = 2;
  f.vm.tm.totalUs = f.vm.tm.remainingUs = 300LL * kUsPerS;
  f.vm.target.set = false;
  f.vm.bat = BatteryState{false, 0, 0};
  SystemView& s = f.vm.sys;
  s.fw = "2.0.0"; s.build = "Oct 11 2026 19:30:12"; s.resetReason = ResetReason::PowerOn; s.wakeCause = WakeCause::None;
  s.uptimeS = 8049; s.heapFree = 271000; s.usbHost = true; s.cdcConnected = true; s.loopMaxUs = 6100; s.flushUs = 3400;
  s.scan0[0] = 0x0D; s.scan0[1] = 0x44; s.n0 = 2; s.scan1[0] = 0x3C; s.n1 = 1;
  f.ui.card = ErrCode::None;
  f.ui.animations = true;
  f.ui.listCursor = SettingId::TimeZone;
  refreshBg(f);
  return f;
}

void renderFixture(const FixtureCtx& fIn, RenderOut& out, DrawRecorder* rec) {
  FixtureCtx f = fIn;
  f.vm.settings = &f.settings;
  HostU8g2 h;
  Canvas c(&h.g);
  FakeDisplay d(&h.g);
  UiController ui(c, d);
  ui.stateForTest() = f.ui;
  for (uint8_t i = 0; i < f.nCards; ++i) ui.showCard(f.cards[i], f.cardArgs[i]);
  if (f.overlay == ScreenId::PowerOff) ui.openPowerOff(f.now);
  if (f.overlay == ScreenId::Splash) ui.showSplash(true);
  c.setRecorder(rec);
  ui.render(f.vm, f.now);
  c.setRecorder(nullptr);
  memcpy(out.buf, c.buffer(), sizeof out.buf);
  out.rejected = ui.rejectedLastFrame();
  ui.bindings(f.vm, out.bindings);
  out.top = ui.topScreen(f.vm);
  ui.toastText(out.toast, sizeof out.toast);
}

}  // namespace hg
