#pragma once
// Icon ids and their ASCII-art bitmaps (SPEC §3.5); bitmaps live in icons.cpp.
#include "../core/types.h"
namespace hg {
enum class IconId : uint8_t {
  None,
  // 5x7 key icons
  Play, Pause, Flag, Reset, Cal, Plus, Check, X, Back, Info, Sleep, Power, Target, Next, Edit, Enter, Retry, Heat, Scan,
  // 5x5 status icons
  SSw, STmr, SWarn, SFlame, SOk, SFail, SBell5,
  // others
  Bell /*7x7*/, Live /*3x5*/, ChevUp /*5x3*/, ChevDown /*5x3*/,
  Count };
struct IconBitmap { uint8_t w, h; const char* rows; };   // rows: h strings of w chars ('#' lit), concatenated
const IconBitmap& iconBitmap(IconId id);                  // None -> 0x0
}  // namespace hg
