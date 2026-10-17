#pragma once
#include "screen_base.h"

class ScreenHome : public ScreenBase {
public:
    void onEnter() override;
    void tick()    override;
    void render()  override;
private:
    uint8_t  lastSec  = 255;
    uint8_t  lastBat  = 254;  // tracks battery % for change detection
};
