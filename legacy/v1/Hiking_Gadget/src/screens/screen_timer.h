#pragma once
#include <stddef.h>
#include "screen_base.h"

class ScreenTimer : public ScreenBase {
public:
    void onEnter()              override;
    void onExit()               override;
    void onButton(ButtonEvent ev) override;
    void tick()                 override;
    void render()               override;
    bool wantsWake()            override;
    bool isBusy()               override { return state != State::SELECT; }

private:
    static const uint16_t PRESETS[];
    static const uint8_t  PRESET_COUNT;

    enum class State : uint8_t { SELECT, RUNNING, ALERT };

    State    state       = State::SELECT;
    uint8_t  presetIdx   = 0;
    uint32_t startMs     = 0;
    uint32_t durationMs  = 0;
    uint32_t alertStart  = 0;
    bool     alertFlip   = false;
    bool     wakeRequest = false;   // set when countdown hits zero while asleep
    uint32_t prevRemain  = 0xFFFFFFFF;

    uint32_t remainingMs() const;
    void     formatRemaining(char* buf, size_t len) const;
};
