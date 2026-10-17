#pragma once
#include "../types.h"

struct TZEntry { const char* name; int16_t offsetMin; };

extern const TZEntry TZ_LIST[];
extern const uint8_t TZ_COUNT;

// Global settings instance — read/write everywhere
extern AppSettings gSettings;

namespace SettingsStore {
    void          begin();
    void          load();
    void          save();
    AppSettings   defaults();
    const char*   tzName(uint8_t index);
    int16_t       tzOffsetMin(uint8_t index);
}
