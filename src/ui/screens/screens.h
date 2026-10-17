#pragma once
// The screen objects (stateless renderers + binding providers, SPEC §4.2 rule 2) and helpers they share.
#include "../screen.h"
#include "../view_model.h"

namespace hg {
class Canvas;
const Screen& homeScreen();
const Screen& stopwatchScreen();
const Screen& timerScreen();
const Screen& alertScreen();
const Screen& cardScreen();
const Screen& powerOffScreen();
const Screen& splashScreen();
const Screen& compassScreen();
const Screen& calibrationScreen();
const Screen& settingsCoverScreen();
const Screen& settingsListScreen();
const Screen& pickerScreen();
const Screen& setTimeScreen();
const Screen& diagnosticsScreen();

// Settings list rows (§3.8.5): section headers (9 px) and items (11 px) from y 10 to 63.
struct ListRow { bool header; Section section; SettingId id; };
uint8_t listRowCount();
ListRow listRow(uint8_t i);
uint8_t listRowOf(SettingId id);
int16_t listRowHeight(uint8_t i);
uint8_t listScroll(uint8_t top, SettingId cursor, int dir);   // new top row: cursor fully visible, +1 look-ahead down
bool listRowEditable(SettingId id, const FormatCtx& ctx);    // Enum/Number row whose value can be edited now

// Shared helpers
bool useFahrenheit(const ViewModel& vm);
const char* keySymbol(uint8_t keyIndex);                          // "^", "v", "#", "*"
void statusClock(const ViewModel& vm, char* b, size_t n);        // "HH:MM" or "" when the clock is not set
Binding pageKey(ActionId a);                                      // ^/v page change (D)
Binding act(ActionId a, IconId icon, uint16_t label = 0, LabelMode m = LabelMode::None);
Binding holdAct(ActionId shortA, ActionId longA, uint16_t holdMs, IconId icon, IconId holdIcon, uint16_t label,
                LabelMode m, uint16_t hint);
}  // namespace hg
