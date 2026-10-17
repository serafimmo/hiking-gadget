#pragma once
#include <stdint.h>

// WiFi is OFF by default and only powered up on demand (NTP sync or the setup
// portal), then switched fully off again — essential for the small ~300mAh pack.

namespace Net {
    bool hasCreds();            // are home-WiFi credentials stored?

    // Try the saved network briefly, run NTP, then power WiFi back off.
    // Draws progress on the OLED. Returns true if the clock got synced.
    bool syncTimeNow();

    // Blocking AP setup portal: user joins "HikeGadget-Setup", picks their home
    // network + password, we save it and sync time. Draws on OLED; K1 exits.
    void startPortal();
}
