#pragma once
#include "../screen.h"
namespace hg {
const Screen& screenFor(ScreenId id);   // registry.cpp: screenForA(id), else screenForB(id), else a null screen that
                                        // draws nothing and binds nothing (and logs once)
const Screen* screenForA(ScreenId id);  // registry_a.cpp: Home, Stopwatch, Timer, Alert, Card, PowerOff, Splash; else nullptr
const Screen* screenForB(ScreenId id);  // registry_b.cpp: Compass, Calibration, SettingsCover, SettingsList, Picker,
                                        // SetTime, Diagnostics; else nullptr
uint32_t nullScreenHits();              // how many lookups fell through to the null screen (logged by the app)
}  // namespace hg
