#pragma once
#include "../types.h"

class ScreenBase {
public:
    virtual ~ScreenBase() {}
    virtual void onEnter() { _dirty = true; }
    virtual void onExit()  {}
    virtual void onButton(ButtonEvent ev) { (void)ev; }
    virtual void tick()    {}   // called every loop (even while the screen is off)
    virtual void render()   = 0;
    // Return true once to request waking the display (e.g. timer finished).
    virtual bool wantsWake() { return false; }
    // Return true while doing time-sensitive work that must not be deep-slept
    // away (running stopwatch / timer).
    virtual bool isBusy()    { return false; }

    bool isDirty()    { return _dirty; }
    void markDirty()  { _dirty = true; }
    void clearDirty() { _dirty = false; }

protected:
    bool _dirty = true;
};
