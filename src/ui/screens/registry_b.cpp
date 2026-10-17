#include "registry.h"
#include "screens.h"

namespace hg {
// Compass, Calibration, SettingsCover, SettingsList, Picker, SetTime, Diagnostics.
const Screen* screenForB(ScreenId id) {
  switch (id) {
    case ScreenId::Compass:       return &compassScreen();
    case ScreenId::Calibration:   return &calibrationScreen();
    case ScreenId::SettingsCover: return &settingsCoverScreen();
    case ScreenId::SettingsList:  return &settingsListScreen();
    case ScreenId::Picker:        return &pickerScreen();
    case ScreenId::SetTime:       return &setTimeScreen();
    case ScreenId::Diagnostics:   return &diagnosticsScreen();
    default:                      return nullptr;
  }
}
}  // namespace hg
