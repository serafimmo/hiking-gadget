#pragma once

namespace Power {
    void begin();              // CPU clock + power init
    void deepSleep();          // OLED off + deep sleep; wakes on any button press
    bool wokeFromDeepSleep();  // true if this boot was a deep-sleep button wake
}
