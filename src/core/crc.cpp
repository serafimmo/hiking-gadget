#include "crc.h"

namespace hg {

uint8_t crc8Sensirion(const uint8_t* d, size_t n) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < n; ++i) {
    crc ^= d[i];
    for (int b = 0; b < 8; ++b) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

uint32_t crc32(const void* d, size_t n, uint32_t seed) {
  const uint8_t* p = static_cast<const uint8_t*>(d);
  uint32_t crc = ~seed;
  for (size_t i = 0; i < n; ++i) {
    crc ^= p[i];
    for (int b = 0; b < 8; ++b) crc = (crc & 1u) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
  }
  return ~crc;
}

uint32_t fnv1a32(const char* s, uint32_t h) {
  for (; *s; ++s) {
    h ^= (uint8_t)*s;
    h *= 16777619u;
  }
  return h;
}

}  // namespace hg
