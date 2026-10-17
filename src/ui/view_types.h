#pragma once
// The types both view_model.h and screen.h need, so neither includes the other.
#include "../core/types.h"
namespace hg {
enum class ScreenId : uint8_t { Home, Stopwatch, Timer, Compass, SettingsCover, SettingsList, Picker, SetTime,
                                Calibration, Diagnostics, Card, Alert, PowerOff, Splash, Count };
struct FormatCtx { ShtChip sht; MagChip mag; float liveHeadingDeg; float liveTempC; int64_t utcS;    // for ui/fmt
                  bool haveHeading, haveTemp; int8_t calPct /*-1 none*/; uint8_t calGrade /*0 good,1 ok*/; };
}  // namespace hg
