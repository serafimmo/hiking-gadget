#include "i2c_buses.h"
#include <Wire.h>
#include "../config.h"

void I2CBuses::begin() {
    Wire.begin(PIN_SDA_SENSOR, PIN_SCL_SENSOR);
    Wire.setClock(400000);

    Wire1.begin(PIN_SDA_DISPLAY, PIN_SCL_DISPLAY);
    Wire1.setClock(400000);
}
