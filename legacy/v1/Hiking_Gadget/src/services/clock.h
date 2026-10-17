#pragma once
#include "../types.h"

namespace Clock {
    void      begin();               // waits for Serial time sync, then starts
    void      setEpoch(uint32_t e);  // set RTC from a UTC unix timestamp (NTP/Serial)
    uint32_t  nowUTC();              // Unix epoch, UTC (from hardware RTC)
    uint32_t  nowLocal();            // UTC + timezone offset
    uint32_t  uptimeMs();            // monotonic ms (RTC-backed; survives light sleep)
    DateTime  toDateTime(uint32_t epoch);
    bool      isSynced();
    const char* weekdayShort(uint8_t wd);  // "Mon" etc.
    const char* monthShort(uint8_t m);     // "Jan" etc.
}
