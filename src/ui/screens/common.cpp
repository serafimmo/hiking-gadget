#include <stdio.h>

#include "../../config/timing.h"
#include "../../core/settings.h"
#include "screens.h"

namespace hg {

bool useFahrenheit(const ViewModel& vm) {
  return vm.settings && optionValue(SettingId::TempUnit, vm.settings->get(SettingId::TempUnit)) == 1;
}

const char* keySymbol(uint8_t k) {
  static const char* const kSym[4] = {"^", "v", "#", "*"};
  return kSym[k & 3];
}

void statusClock(const ViewModel& vm, char* b, size_t n) {
  if (!vm.clock.valid) { if (n) b[0] = 0; return; }
  snprintf(b, n, "%02d:%02d", vm.clock.local.hour, vm.clock.local.minute);
}

Binding pageKey(ActionId a) {
  Binding b;
  b.shortAct = a;
  return b;
}

Binding act(ActionId a, IconId icon, uint16_t label, LabelMode m) {
  Binding b;
  b.shortAct = a;
  b.icon = icon;
  b.labelStr = label;
  b.labelMode = label ? m : LabelMode::None;
  return b;
}

Binding holdAct(ActionId shortA, ActionId longA, uint16_t holdMs, IconId icon, IconId holdIcon, uint16_t label,
                LabelMode m, uint16_t hint) {
  Binding b;
  b.shortAct = shortA;
  b.longAct = longA;
  b.holdMs = holdMs;
  b.icon = icon;
  b.holdIcon = holdIcon;
  b.labelStr = label;
  b.labelMode = label ? m : LabelMode::None;
  b.hintStr = hint;
  return b;
}

}  // namespace hg
