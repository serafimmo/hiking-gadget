#pragma once
// IDisplay is owned by ui/ (re-exported by app/ports.h), so ui/ never includes app/.
#include "../core/types.h"
#include "u8g2_inc.h"
namespace hg {
struct IDisplay {
  virtual bool ok() const = 0;                                   // false -> headless (no frames)
  virtual u8g2_t* u8g2() = 0;
  virtual void sendPageSpan(uint8_t page, uint8_t tx0, uint8_t tw) = 0;   // u8g2_UpdateDisplayArea(tx0, page, tw, 1)
  virtual void setPower(bool on) = 0;  virtual void setLevel(Brightness b) = 0;  virtual void setContrastRaw(uint8_t v) = 0;
  virtual void setInvert(bool on) = 0;  virtual void deepOff() = 0;            // 0xAE + 0x8D 0x10
  virtual bool retryInit() = 0;                                  // zero-length probe first; full init only on ACK (§6.6)
  virtual uint32_t ioErrors() const = 0;
};
}  // namespace hg
