#pragma once
#include "../types.h"

namespace Buttons {
    void begin();
    void poll();
    bool       hasEvent();
    ButtonEvent dequeue();
}
