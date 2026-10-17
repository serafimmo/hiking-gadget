#pragma once
#include <stddef.h>
#include "screen_base.h"

class ScreenStopwatch : public ScreenBase {
public:
    void onEnter()              override;
    void onButton(ButtonEvent ev) override;
    void tick()                 override;
    void render()               override;
    bool isBusy()               override { return state == State::RUNNING; }

private:
    enum class State : uint8_t { IDLE, RUNNING, PAUSED };

    State    state        = State::IDLE;
    uint32_t startMs      = 0;   // millis() when last started
    uint32_t accumulatedMs = 0;  // ms accumulated before last pause

    static const uint8_t MAX_LAPS = 8;
    uint32_t laps[MAX_LAPS];
    uint8_t  lapCount     = 0;

    uint32_t elapsedMs() const;
    void     formatTime(char* buf, size_t len, uint32_t ms) const;
    uint32_t prevElapsed  = 0;
};
