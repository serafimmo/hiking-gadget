#pragma once
#include "../types.h"

namespace CompassDriver {
    void        begin();
    void        tick();
    CompassData getData();
    bool        angleChanged(); // true if smoothed angle moved enough to redraw

    // Hard-iron calibration (rotate the device through a full turn):
    void        calibStart();          // reset min/max accumulators
    void        calibSample();         // read once, update min/max
    int16_t     calibSpan();           // smaller of X/Y ranges — coverage metric
    void        calibApply();          // store offsets in settings + persist
}
