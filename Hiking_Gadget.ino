// Hiking_Gadget.ino — composition only; all code lives in src/ (ctags is stubbed: setup()/loop() only)
#include <U8g2lib.h>       // makes the Arduino builder add U8g2 to the include path for src/
#include <Wire.h>
#include <Preferences.h>
#include "src/hal/board.h"
void setup() { hg::boardSetup(__DATE__, __TIME__); }   // build stamp captured here (§5.2)
void loop()  { hg::boardLoop(); }
