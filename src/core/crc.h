#pragma once
#include "types.h"
namespace hg {
uint8_t  crc8Sensirion(const uint8_t* d, size_t n);         // poly 0x31, init 0xFF, no reflection, xorout 0
uint32_t crc32(const void* d, size_t n, uint32_t seed = 0); // IEEE (reflected 0xEDB88320): "123456789" -> 0xCBF43926
uint32_t fnv1a32(const char* s, uint32_t h = 2166136261u);  // "Oct 11 2026 19:30:12" -> 0x1880C947
}  // namespace hg
