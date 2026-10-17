#pragma once
#include <stdint.h>

// Battery voltage divider on GPIO2:
//   VBAT ──[R1=100k]──┬──[R2=100k]── GND
//                     └── GPIO2 (ADC)
// With equal resistors: V_adc = VBAT / 2
// LiPo range: 3.0V (0%) .. 4.2V (100%)
// ESP32-S3 ADC: 12-bit (0..4095), reference ~3.3V

namespace Battery {
    void    begin();
    void    tick();
    uint8_t percent();   // 0..100, 255 if not available
    float   voltage();   // volts, 0.0 if not available
}
