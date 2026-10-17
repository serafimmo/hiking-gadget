#pragma once
// Page slide composition (SPEC §3.6 motion 1): one uint64 per column, bit = row.
#include "../core/types.h"

namespace hg {
uint64_t frameColumn(const uint8_t* buf, uint8_t x);              // 1024-byte page-major buffer, LSB = top row
void setFrameColumn(uint8_t* buf, uint8_t x, uint64_t v);
// dir > 0 (v, content moves up): out = (old >> d) | (new << (64 - d)); dir < 0 (^): out = (old << d) | (new >> (64 - d)).
uint64_t slideColumn(uint64_t oldCol, uint64_t newCol, uint8_t d, int8_t dir);
// Composes columns 0..x1 of 'live' (holding the new frame) with the frozen 'old' frame, in place. The rail is untouched.
void composeSlide(const uint8_t* oldFrame, uint8_t* live, uint8_t d, int8_t dir, uint8_t x1);
uint8_t slideOffset(int64_t elapsedUs, int64_t durationUs);       // ease-out cubic: round(64 * (1 - (1 - t)^3))
}  // namespace hg
