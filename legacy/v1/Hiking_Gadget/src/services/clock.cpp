#include "clock.h"
#include "settings_store.h"
#include "../config.h"
#include <Arduino.h>
#include <sys/time.h>

// Time is kept in the ESP32 hardware RTC (settimeofday/gettimeofday). The RTC
// keeps running through light sleep, so both the clock and the stopwatch/timer
// (via uptimeMs) stay correct while the CPU is asleep.

static bool synced = false;

static const char* WDAY[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char* MON[]  = {"","Jan","Feb","Mar","Apr","May","Jun",
                              "Jul","Aug","Sep","Oct","Nov","Dec"};

void Clock::setEpoch(uint32_t e) {
    struct timeval tv = { (time_t)e, 0 };
    settimeofday(&tv, nullptr);
    synced = (e > 1700000000UL);
    if (synced) {
        gSettings.lastEpoch = e;
        SettingsStore::save();
    }
}

uint32_t Clock::nowUTC() {
    return (uint32_t)time(nullptr);
}

uint32_t Clock::nowLocal() {
    int32_t off = (int32_t)SettingsStore::tzOffsetMin(gSettings.tzIndex) * 60L;
    return (uint32_t)((int32_t)nowUTC() + off);
}

uint32_t Clock::uptimeMs() {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (uint32_t)((uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000ULL);
}

bool Clock::isSynced() { return synced; }

const char* Clock::weekdayShort(uint8_t wd) { return WDAY[wd % 7]; }
const char* Clock::monthShort(uint8_t m)    { return (m>=1&&m<=12)?MON[m]:"???"; }

static bool isLeap(uint16_t y) { return (y%4==0)&&(y%100!=0||y%400==0); }

DateTime Clock::toDateTime(uint32_t epoch) {
    static const uint8_t DIM[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    DateTime dt;
    dt.weekday = (uint8_t)((epoch / 86400UL + 4) % 7); // 1970-01-01 = Thu = 4
    uint32_t days = epoch / 86400UL;
    uint32_t secs = epoch % 86400UL;
    dt.hour   = secs / 3600;
    dt.minute = (secs % 3600) / 60;
    dt.second = secs % 60;
    uint16_t y = 1970;
    while (true) {
        uint32_t diy = isLeap(y) ? 366 : 365;
        if (days < diy) break;
        days -= diy;
        y++;
    }
    dt.year = y;
    uint8_t m = 1;
    while (m <= 12) {
        uint8_t dim = DIM[m];
        if (m == 2 && isLeap(y)) dim = 29;
        if (days < dim) break;
        days -= dim;
        m++;
    }
    dt.month = m;
    dt.day   = (uint8_t)(days + 1);
    return dt;
}

void Clock::begin() {
    // If the RTC already holds a valid time (e.g. we just woke from deep sleep,
    // where the RTC keeps running), trust it and don't overwrite. Otherwise seed
    // it from the last saved epoch so the clock is at least plausible.
    if (time(nullptr) > 1700000000UL) {
        synced = true;
        return;   // RTC valid → skip the Serial wait so a wake feels instant
    } else if (gSettings.lastEpoch > 1700000000UL) {
        struct timeval tv = { (time_t)gSettings.lastEpoch, 0 };
        settimeofday(&tv, nullptr);
    }

    // Accept an optional "T<unix_timestamp>\n" pushed over Serial at boot.
    uint32_t deadline = millis() + CLOCK_SYNC_TIMEOUT_MS;
    while (millis() < deadline) {
        if (Serial.available()) {
            if (Serial.peek() == 'T') {
                Serial.read();
                uint32_t ts = (uint32_t)Serial.parseInt();
                if (ts > 1700000000UL) { setEpoch(ts); return; }
            } else {
                Serial.read();
            }
        }
    }
}
