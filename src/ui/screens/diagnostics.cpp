// DIAGNOSTICS (SPEC §3.8.7, §9.5). Micro text is allowed here.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../../config/board_pins.h"
#include "../../config/timing.h"
#include "../canvas.h"
#include "../strings.h"
#include "../theme.h"
#include "../widgets.h"
#include "screens.h"

namespace hg {
namespace {
const char* chipName(ShtChip c) { return c == ShtChip::Sht4x ? "SHT4X" : c == ShtChip::Sht3x ? "SHT3X" : c == ShtChip::Unknown ? "UNKNOWN" : "NONE"; }
const char* chipName(MagChip c) {
  switch (c) {
    case MagChip::Qmc5883l: return "QMC5883L";
    case MagChip::Qmc5883p: return "QMC5883P";
    case MagChip::Hmc5883l: return "HMC5883L";
    case MagChip::Unknown:  return "UNKNOWN";
    default:                return "NONE";
  }
}
const char* resetName(ResetReason r) {
  static const char* const k[16] = {"UNKNOWN", "POWERON", "EXT", "SW", "PANIC", "INT_WDT", "TASK_WDT", "WDT",
                                    "DEEPSLEEP", "BROWNOUT", "SDIO", "USB", "JTAG", "EFUSE", "PWR_GLITCH", "CPU_LOCKUP"};
  return (uint8_t)r < 16 ? k[(uint8_t)r] : "?";
}
const char* addrName(uint8_t a) {
  switch (a) {
    case 0x0D: return "QMC";
    case 0x2C: return "QMCP";
    case 0x1E: return "HMC";
    case 0x44: case 0x45: return "SHT";
    case 0x3C: case 0x3D: return "OLED";
    default:   return "?";
  }
}

void row(Canvas& c, int i, const char* key, const char* val) {
  c.font(Font::Micro);
  const int16_t y = (int16_t)(12 + 9 * i);
  c.text(0, y, key);
  c.text(24, y, val);
}

void pageSensors(Canvas& c, const ViewModel& vm) {
  char v[64];
  snprintf(v, sizeof v, "SSD1306 3C OK");
  row(c, 0, "OLED", v);
  if (vm.thermo.chip == ShtChip::Sht4x) snprintf(v, sizeof v, "SHT4X %02X %08lX", vm.thermo.addr, (unsigned long)vm.thermo.serial);
  else snprintf(v, sizeof v, "%s %02X", chipName(vm.thermo.chip), vm.thermo.addr);
  row(c, 1, "TEMP", v);
  snprintf(v, sizeof v, "%s %02X ID %02X%s", chipName(vm.mag.chip), vm.mag.addr, vm.mag.idByte, vm.mag.idWarning ? "?" : "");
  row(c, 2, "MAG", v);
  if (vm.mag.drdy == DrdyKind::None) snprintf(v, sizeof v, "NO PIN (QMC5883P)");
  else snprintf(v, sizeof v, "GPIO%u %lu/S %s", kPinMagDrdy, (unsigned long)vm.mag.drdyPerS,
                vm.mag.drdy == DrdyKind::RisingLevel ? "RISE" : "FALL");
  row(c, 3, "DRDY", v);
  snprintf(v, sizeof v, "ERR %lu/%lu REC %lu", (unsigned long)vm.sys.bus0.fail, (unsigned long)vm.sys.bus1.fail,
           (unsigned long)vm.sys.bus0.recoveries);
  row(c, 4, "I2C", v);
  if (vm.sys.heatValid) {
    const int pre = vm.sys.heatPreC10, hot = vm.sys.heatHotC10, d = hot - pre;
    snprintf(v, sizeof v, "%d.%d>%d.%d D%+d.%d", pre / 10, pre % 10, hot / 10, hot % 10, d / 10, (d < 0 ? -d : d) % 10);
  } else {
    snprintf(v, sizeof v, "-");
  }
  row(c, 5, "HEAT", v);
}

void pageI2c(Canvas& c, const ViewModel& vm) {
  char v[48];
  c.font(Font::Micro);
  c.text(0, 12, "BUS0 SDA8 SCL7 100K");
  int16_t x = 0;
  for (uint8_t i = 0; i < vm.sys.n0 && i < 8; ++i) {
    snprintf(v, sizeof v, "%02X", vm.sys.scan0[i]);
    c.font(Font::Body);
    const int16_t w = c.num(x, 20, v);
    c.font(Font::Micro);
    c.text(x, 29, addrName(vm.sys.scan0[i]));
    x = (int16_t)(x + (w > 16 ? w : 16) + 6);
  }
  if (!vm.sys.n0) { c.font(Font::Body); c.text(0, 20, "-"); }
  c.font(Font::Micro);
  c.text(0, 38, "BUS1 SDA10 SCL11 400K");
  v[0] = 0;
  for (uint8_t i = 0; i < vm.sys.n1 && i < 4; ++i) {
    char one[12];
    snprintf(one, sizeof one, "%02X %s ", vm.sys.scan1[i], addrName(vm.sys.scan1[i]));
    strncat(v, one, sizeof v - strlen(v) - 1);
  }
  c.text(0, 46, v[0] ? v : "-");
  c.text(0, 56, "SCANNED 0X08-0X77");
}

void pageButtons(Canvas& c, const ViewModel& vm) {
  static const char* const kName[4] = {"UP", "DOWN", "HASH", "STAR"};
  char v[40];
  c.font(Font::Micro);
  for (int i = 0; i < 4; ++i) {
    snprintf(v, sizeof v, "K%d GPIO%-2u %s %-4s %u", i + 1, kKeyPins[i], i == 1 ? "V" : keySymbol((uint8_t)i), kName[i],
             vm.sys.keyPresses[i]);
    c.text(0, (int16_t)(12 + 10 * i), v);
  }
  c.font(Font::Body);
  c.text(0, 54, str(S_DIAG_EXIT));
  c.scope(DrawScope::Rail);   // rail boxes next to ^ v # *: shows the mapping and the rail alignment at once
  for (int i = 0; i < 4; ++i) {
    const Rect r{kRailX, kRailSlotY[i], 9, 9};
    if (vm.sys.keyDownMask & (1u << i)) c.rbox(r, 1);
    else c.rframe(r, 1);
  }
  c.scope(DrawScope::Content);
}

void pageMag(Canvas& c, const ViewModel& vm) {
  static const char* const kAxis[3] = {"X", "Y", "Z"};
  char v[40];
  for (int i = 0; i < 3; ++i) {
    const int16_t y = (int16_t)(12 + 10 * i);
    c.font(Font::Micro);
    c.text(0, (int16_t)(y + 1), kAxis[i]);
    snprintf(v, sizeof v, "%d", vm.mag.raw[i]);
    c.font(Font::Body);
    c.num(10, y, v);
    snprintf(v, sizeof v, "%d..%d", vm.mag.mn[i], vm.mag.mx[i]);
    c.font(Font::Micro);
    c.text(56, (int16_t)(y + 1), v);
  }
  c.font(Font::Micro);
  snprintf(v, sizeof v, "BH %d.%dUT  RATE %u/S  OVF %d", (int)vm.mag.fieldUt, (int)(vm.mag.fieldUt * 10) % 10,
           vm.mag.rateHz, vm.mag.saturated ? 1 : 0);
  c.text(0, 44, v);
  snprintf(v, sizeof v, "HDG RAW %05.1f  CAL %05.1f", (double)vm.mag.headingDeg, (double)vm.mag.cardDeg);
  c.text(0, 53, v);
}

void pageSystem(Canvas& c, const ViewModel& vm) {
  char v[48];
  const SystemView& s = vm.sys;
  c.font(Font::Micro);
  int i = 0;
  auto line = [&](const char* t) { c.text(0, (int16_t)(11 + 7 * i++), t); };
  char build[24] = "";
  if (s.build) { strncpy(build, s.build, 11); build[11] = 0; for (char* p = build; *p; ++p) if (*p >= 'a' && *p <= 'z') *p = (char)(*p - 32); }
  snprintf(v, sizeof v, "FW %s %s", s.fw ? s.fw : "?", build);
  line(v);
  snprintf(v, sizeof v, "CORE %s", s.coreVer ? s.coreVer : "HOST");
  line(v);
  snprintf(v, sizeof v, "RESET %s", resetName(s.resetReason));
  line(v);
  if (s.wakeCause == WakeCause::Ext1) snprintf(v, sizeof v, "WAKE EXT1 K%u +%luMS", s.wakeKey + 1u, (unsigned long)(s.wakeSampleUs / 1000));
  else if (s.wakeCause == WakeCause::Timer) snprintf(v, sizeof v, "WAKE TIMER");
  else snprintf(v, sizeof v, "WAKE -");
  line(v);
  const long up = (long)s.uptimeS;
  snprintf(v, sizeof v, "UPTIME %ldD %02ld:%02ld:%02ld", up / 86400, up / 3600 % 24, up / 60 % 60, up % 60);
  line(v);
  snprintf(v, sizeof v, "USB HOST %s  CDC %s", s.usbHost ? "ON" : "OFF", s.cdcConnected ? "ON" : "OFF");
  line(v);
  snprintf(v, sizeof v, "LOOP MAX %lu.%luMS FLUSH %lu.%luMS", (unsigned long)(s.loopMaxUs / 1000),
           (unsigned long)(s.loopMaxUs / 100 % 10), (unsigned long)(s.flushUs / 1000), (unsigned long)(s.flushUs / 100 % 10));
  line(v);
}

class DiagnosticsScreen final : public Screen {
 public:
  void bindings(const ViewModel& vm, const UiState& ui, Bindings& b) const override {
    if (ui.diagPage == 2) {   // Buttons: every key captured; any key held 2 s exits (handled by UiController)
      b.captureAll = true;
      return;
    }
    b.key[(int)Key::Up] = pageKey(ActionId::DiagPrev);
    b.key[(int)Key::Down] = pageKey(ActionId::DiagNext);
    b.chevrons = true;
    b.key[(int)Key::Hash] = act(ActionId::DiagBack, IconId::Back);
    switch (ui.diagPage) {
      case 0: if (vm.thermo.chip == ShtChip::Sht4x) b.key[(int)Key::Star] = act(ActionId::HeaterTest, IconId::Heat); break;
      case 1: b.key[(int)Key::Star] = act(ActionId::Rescan, IconId::Scan); break;
      case 3: b.key[(int)Key::Star] = act(ActionId::MagMinMaxReset, IconId::Reset); break;
      case 4:
        b.key[(int)Key::Star] = holdAct(ActionId::ShowHint, ActionId::Reboot, kHoldCriticalMs, IconId::Power, IconId::None,
                                        S_L_HOLD, LabelMode::PillOnHold, S_HINT_HOLD_REBOOT);
        break;
      default: break;
    }
  }
  void render(Canvas& c, const ViewModel& vm, const UiState& ui, MonoUs) const override {
    static const uint16_t kTitles[5] = {S_DIAG_SENSORS, S_DIAG_I2C, S_DIAG_BUTTONS, S_DIAG_MAG, S_DIAG_SYSTEM};
    const uint8_t p = ui.diagPage % 5;
    char t[24], pos[8];
    snprintf(t, sizeof t, str(S_DIAG_TITLE), str(kTitles[p]));
    snprintf(pos, sizeof pos, "%u/5", p + 1u);
    c.scope(DrawScope::Status);
    c.font(Font::Micro);
    c.text(0, kStatusInk, t);
    c.num(p == 2 ? 104 : kContentX1, kStatusInk, pos, Align::Right);   // Buttons: header ends at x 104
    c.dottedH(0, kStatusRuleY, p == 2 ? 105 : kContentX1 + 1);
    c.scope(DrawScope::Content);
    switch (p) {
      case 0: pageSensors(c, vm); break;
      case 1: pageI2c(c, vm); break;
      case 2: pageButtons(c, vm); break;
      case 3: pageMag(c, vm); break;
      default: pageSystem(c, vm); break;
    }
  }
  uint16_t framePeriodMs(const ViewModel&, const UiState& ui) const override { return ui.diagPage == 2 ? 50 : 200; }
  uint8_t sensorDemand(const ViewModel&, const UiState& ui) const override {
    return (ui.diagPage == 0 || ui.diagPage == 3) ? 1 : 0;
  }
};
const DiagnosticsScreen kDiag;
}  // namespace

const Screen& diagnosticsScreen() { return kDiag; }

}  // namespace hg
