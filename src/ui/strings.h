#pragma once
// Frame of the string table (§4.10). The text lives in the X-macro fragments strings_core.h (UI foundation),
// strings_screens_a.h (Home, Stopwatch, Timer, cards) and strings_screens_b.h (Compass, Settings, Diagnostics).
// Columns: HG_STR(id, role, English text, worst-case argument for "%s" or ""). Printable ASCII only (D29).
#include "../core/types.h"
namespace hg {
enum class StrRole : uint8_t { Toast, CardTitle, CardRow, Row, Label, Micro, Free };  // test_strings maps role -> font + max width
enum StrId : uint16_t {
#define HG_STR(id, role, en, worst) id,
  S_NONE,
#include "strings_core.h"
#include "strings_screens_a.h"
#include "strings_screens_b.h"
#undef HG_STR
  S_COUNT };
const char* str(uint16_t id);          // table built the same way in strings.cpp
StrRole     strRole(uint16_t id);
const char* strWorstArg(uint16_t id);  // worst-case argument for a "%s" template ("" if none); used by test_strings
uint16_t errTitle(ErrCode c);          // declared here, defined in ui/err_text.cpp
uint16_t errHint(ErrCode c);
}  // namespace hg
