#pragma once
#include "screen_base.h"

class ScreenSettings : public ScreenBase {
public:
    void onEnter()                override;
    void onExit()                 override;
    void onButton(ButtonEvent ev) override;
    void tick()                   override;
    void render()                 override;

private:
    // Menu items
    enum Item : uint8_t {
        IT_TZ = 0, IT_TIMEOUT, IT_HEATER, IT_LOWPOWER,
        IT_DECL, IT_CALIB, IT_SYNC, IT_WIFI, IT_POWEROFF, ITEM_COUNT
    };
    static const uint8_t VISIBLE = 4;   // rows shown at once

    enum class Mode : uint8_t { LIST, CALIBRATING };
    Mode     mode         = Mode::LIST;
    uint8_t  selectedItem = 0;
    uint8_t  scrollTop    = 0;
    uint32_t calibStartMs = 0;
    uint32_t lastSampleMs = 0;

    static const uint16_t TIMEOUT_OPTS[];
    static const uint8_t  TIMEOUT_COUNT;
    static const uint8_t  HEATER_OPTS[];
    static const uint8_t  HEATER_COUNT;

    template<typename T>
    uint8_t findIdx(const T* arr, uint8_t n, T val) {
        for (uint8_t i = 0; i < n; i++) if (arr[i] == val) return i;
        return 0;
    }

    void activate(uint8_t item);
    void itemText(uint8_t item, const char*& label, char* val, uint8_t vlen);
    void renderList();
    void renderCalib();
};
