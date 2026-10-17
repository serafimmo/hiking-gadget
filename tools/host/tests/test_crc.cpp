// CRC and hash vectors (SPEC Appendix B).
#include "core/crc.h"
#include "tiny_test.h"

using namespace hg;

TEST(crc8_sensirion_vectors) {
  const uint8_t v[5][2] = {{0xBE, 0xEF}, {0x66, 0x66}, {0x80, 0x00}, {0x00, 0x00}, {0xFF, 0xFF}};
  const uint8_t want[5] = {0x92, 0x93, 0xA2, 0x81, 0xAC};
  for (int i = 0; i < 5; ++i) CHECK_EQ(crc8Sensirion(v[i], 2), want[i]);
}

TEST(crc32_ieee_check_value) {
  CHECK_EQ(crc32("123456789", 9), 0xCBF43926u);
  // Chained: crc32(b, seed = crc32(a)) == crc32(a + b)
  CHECK_EQ(crc32("6789", 4, crc32("12345", 5)), 0xCBF43926u);
}

TEST(fnv1a32_build_stamp) {
  CHECK_EQ(fnv1a32("Oct 11 2026 19:30:12"), 0x1880C947u);
  CHECK_EQ(fnv1a32(""), 2166136261u);
}
