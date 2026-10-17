#pragma once
// Register maps and decoders of the supported magnetometers (SPEC §6.3). Values from the datasheets, not from
// third-party drivers [HWF THIRDPARTY-BUGS].
#include "../core/types.h"

namespace hg {
namespace qmcl {   // QMC5883L @0x0D
constexpr uint8_t kData = 0x00, kStatus = 0x06, kCtrl1 = 0x09, kCtrl2 = 0x0A, kSetReset = 0x0B, kChipId = 0x0D;
constexpr uint8_t kSoftReset = 0x80, kSetResetPeriod = 0x01, kCtrl2Run = 0x00 /*INT_ENB=0: DRDY pin on*/;
constexpr uint8_t kCtrl1Run = 0x15;   // OSR 512, RNG 8 G, ODR 50 Hz, continuous
constexpr uint8_t kCtrl1Standby = kCtrl1Run & ~0x03;
constexpr uint8_t kIdExpected = 0xFF;
constexpr uint8_t kDrdy = 0x01, kOvl = 0x02;
}  // namespace qmcl
namespace qmcp {   // QMC5883P @0x2C (QST datasheet Rev E)
constexpr uint8_t kChipIdReg = 0x00, kData = 0x01, kStatus = 0x09, kCtrl1 = 0x0A, kCtrl2 = 0x0B, kAxisSign = 0x29;
constexpr uint8_t kSoftReset = 0x80, kCtrl2Run = 0x08 /*set/reset on, RNG 8 G*/, kAxisSignValue = 0x06;
constexpr uint8_t kSuspend = 0x00, kCtrl1Run = 0xC5;   // OSR2 8, OSR1 8, ODR 50 Hz, Normal
constexpr uint8_t kIdExpected = 0x80;
constexpr uint8_t kDrdy = 0x01, kOvfl = 0x02;
}  // namespace qmcp
namespace hmc {    // HMC5883L @0x1E (and the "DA 5883" clone)
constexpr uint8_t kCra = 0x00, kCrb = 0x01, kMode = 0x02, kData = 0x03, kStatus = 0x09, kId = 0x0A;
constexpr uint8_t kCraRun = 0x70 /*8 avg, 15 Hz*/, kCrbRun = 0x20 /*+-1.3 Ga*/, kModeCont = 0x00, kModeIdle = 0x02;
constexpr uint8_t kRdy = 0x01;
constexpr int16_t kOverflow = -4096;
}  // namespace hmc

struct MagChipInfo { MagChip chip; uint8_t addr; float lsbPerGauss; DrdyKind drdy; uint32_t odrUs; uint32_t resetWaitUs;
                     const char* name; };
const MagChipInfo& magChipInfo(MagChip c);

// Decoders: 6 data bytes -> BOARD frame (HMC/QMC5883L axis convention). Return true on overflow.
bool decodeQmcL(const uint8_t* b, uint8_t status, int16_t& x, int16_t& y, int16_t& z);   // X,Y,Z int16 LE
bool decodeQmcP(const uint8_t* b, uint8_t status, int16_t& x, int16_t& y, int16_t& z);   // LE; board = (py, -px, pz)
bool decodeHmc(const uint8_t* b, int16_t& x, int16_t& y, int16_t& z);                    // X,Z,Y int16 BE; -4096 = ovf
}  // namespace hg
