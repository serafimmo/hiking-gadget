#include "registry.h"
#include "screens.h"

namespace hg {
// Home, Stopwatch, Timer, Alert, Card, PowerOff, Splash.
const Screen* screenForA(ScreenId id) {
  switch (id) {
    case ScreenId::Home:      return &homeScreen();
    case ScreenId::Stopwatch: return &stopwatchScreen();
    case ScreenId::Timer:     return &timerScreen();
    case ScreenId::Alert:     return &alertScreen();
    case ScreenId::Card:      return &cardScreen();
    case ScreenId::PowerOff:  return &powerOffScreen();
    case ScreenId::Splash:    return &splashScreen();
    default:                  return nullptr;
  }
}
}  // namespace hg
