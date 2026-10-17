#pragma once
// The only file in ui/ that branches on ARDUINO. Both builds use the C API only: on the device the library's
// clib header (the .ino includes <U8g2lib.h>, which makes the builder add U8g2/src to the include path), on the
// host the same clib via -I. Never U8g2lib.h here: it drags Arduino.h (and its F() macro) into ui/.
#ifdef ARDUINO
#include <clib/u8g2.h>
#else
#include "u8g2.h"
#endif
