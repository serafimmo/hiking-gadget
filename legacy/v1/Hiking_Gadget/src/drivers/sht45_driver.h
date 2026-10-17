#pragma once
#include "../types.h"

namespace SHT45Driver {
    void       begin();
    void       tick();
    SensorData getData();
}
