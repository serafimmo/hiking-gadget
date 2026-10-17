#pragma once
#include "types.h"
namespace hg {
float sht4xTempC(uint16_t raw);  float sht4xRh(uint16_t raw);   // RH clamped 0..100
float sht3xTempC(uint16_t raw);  float sht3xRh(uint16_t raw);   // RH = 100*S/65535
float correctRhForTemp(float rhRaw, float tRawC, float tCorrC);  // rh*es(tRaw)/es(tCorr), clamp 0..100 (dew point kept)
float dewPointC(float tC, float rh);                             // Magnus a = 17.62, b = 243.12 C
float absHumidity(float tC, float rh);                           // g/m3
float cToF(float c);
}  // namespace hg
