#include "env_math.h"

#include <math.h>

namespace hg {
namespace {
float clampRh(float rh) { return rh < 0 ? 0 : rh > 100 ? 100 : rh; }
float satVaporHpa(float tC) { return 6.112f * expf(17.62f * tC / (243.12f + tC)); }   // Magnus, over water
}  // namespace

float sht4xTempC(uint16_t raw) { return -45.0f + 175.0f * (float)raw / 65535.0f; }
float sht4xRh(uint16_t raw) { return clampRh(-6.0f + 125.0f * (float)raw / 65535.0f); }
float sht3xTempC(uint16_t raw) { return -45.0f + 175.0f * (float)raw / 65535.0f; }
float sht3xRh(uint16_t raw) { return clampRh(100.0f * (float)raw / 65535.0f); }

float correctRhForTemp(float rhRaw, float tRawC, float tCorrC) {
  return clampRh(rhRaw * satVaporHpa(tRawC) / satVaporHpa(tCorrC));
}

float dewPointC(float tC, float rh) {
  const float r = rh < 0.1f ? 0.1f : rh;
  const float g = logf(r / 100.0f) + 17.62f * tC / (243.12f + tC);
  return 243.12f * g / (17.62f - g);
}

float absHumidity(float tC, float rh) { return 216.7f * (clampRh(rh) / 100.0f * satVaporHpa(tC)) / (273.15f + tC); }

float cToF(float c) { return c * 9.0f / 5.0f + 32.0f; }

}  // namespace hg
