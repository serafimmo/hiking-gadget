#include "mag_chips.h"

namespace hg {
namespace {
const MagChipInfo kInfo[] = {
    {MagChip::None,     0x00, 1.0f,    DrdyKind::None,         20000, 0,     "NONE"},
    {MagChip::Qmc5883l, 0x0D, 3000.0f, DrdyKind::RisingLevel,  20000, 10000, "QMC5883L"},
    {MagChip::Qmc5883p, 0x2C, 3750.0f, DrdyKind::None,         20000, 5000,  "QMC5883P"},
    {MagChip::Hmc5883l, 0x1E, 1090.0f, DrdyKind::FallingPulse, 66667, 0,     "HMC5883L"},
    {MagChip::Unknown,  0x00, 1.0f,    DrdyKind::None,         20000, 0,     "UNKNOWN"},
};
int16_t le(const uint8_t* b) { return (int16_t)(uint16_t)(b[0] | (b[1] << 8)); }
int16_t be(const uint8_t* b) { return (int16_t)(uint16_t)((b[0] << 8) | b[1]); }
int16_t neg(int16_t v) { return v == -32768 ? 32767 : (int16_t)-v; }
}  // namespace

const MagChipInfo& magChipInfo(MagChip c) { return kInfo[(uint8_t)c <= (uint8_t)MagChip::Unknown ? (uint8_t)c : 0]; }

bool decodeQmcL(const uint8_t* b, uint8_t status, int16_t& x, int16_t& y, int16_t& z) {
  x = le(b); y = le(b + 2); z = le(b + 4);
  return (status & qmcl::kOvl) != 0;
}

bool decodeQmcP(const uint8_t* b, uint8_t status, int16_t& x, int16_t& y, int16_t& z) {
  const int16_t px = le(b), py = le(b + 2), pz = le(b + 4);
  x = py; y = neg(px); z = pz;            // the P die is rotated 90 deg on the same footprint [HWF P-AXES] [HW?]
  return (status & qmcp::kOvfl) != 0;
}

bool decodeHmc(const uint8_t* b, int16_t& x, int16_t& y, int16_t& z) {
  x = be(b); z = be(b + 2); y = be(b + 4);
  return x == hmc::kOverflow || y == hmc::kOverflow || z == hmc::kOverflow;
}

}  // namespace hg
