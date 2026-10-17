#pragma once
#include <stdint.h>
#include "../config.h"

// Serial diagnostics for hardware bring-up. Everything prints over Serial at
// 115200. On a USB-CDC build the telemetry stream is silent unless a host is
// attached, so it costs almost nothing in the field; on battery you can also
// flip LOG_ENABLED to 0 in config.h to compile it out entirely.
//
// Line format:  [t+12.345s][I][TAG] message
//   t+  = milliseconds since boot (true uptime, independent of the RTC)
//   I/W/E = info / warning / error

namespace Log {
    void begin();        // banner: chip, reset + wake reason, CPU clock, heap
    void i2cScanAll();   // scan both I2C buses; flag the expected devices
    void event(const char* tag, const char* fmt, ...);
    void warn (const char* tag, const char* fmt, ...);
    void err  (const char* tag, const char* fmt, ...);
    void telemetry();    // periodic one-line sensor snapshot (interval-gated)
}

#if LOG_ENABLED
    #define LOGI(tag, ...) Log::event(tag, __VA_ARGS__)
    #define LOGW(tag, ...) Log::warn (tag, __VA_ARGS__)
    #define LOGE(tag, ...) Log::err  (tag, __VA_ARGS__)
#else
    #define LOGI(tag, ...) do {} while (0)
    #define LOGW(tag, ...) do {} while (0)
    #define LOGE(tag, ...) do {} while (0)
#endif
