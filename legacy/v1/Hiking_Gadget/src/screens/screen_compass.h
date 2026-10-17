#pragma once
#include "screen_base.h"

class ScreenCompass : public ScreenBase {
public:
    void onEnter()              override;
    void onButton(ButtonEvent ev) override;
    void tick()                 override;
    void render()               override;

private:
    int16_t targetBrg = -1;   // -1 = no target set; else 0..359 bearing to hold

    void drawRose();
    void drawNeedle(float angleDeg);
    void drawTargetMark(float bearingDeg);
    void drawCardinal(const char* s, int16_t px, int16_t py);
    static const char* cardinalName(float deg);
};
