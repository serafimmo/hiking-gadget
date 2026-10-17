// Motion and toast fixtures (SPEC §10.2, WP10 group 9). The chrome-only states render already with the null screens.
#include "fixtures.h"
#include "ui/strings.h"

using namespace hg;

HG_FIXTURE(9, toast_hint) { setToast(f, S_T_HOLD_POWER, nullptr); }
HG_FIXTURE(9, toast_longest) { setToast(f, S_T_TIME_SET_D, "-9999.9 s"); }
