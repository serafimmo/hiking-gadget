#pragma once
#include "bindings.h"
#include "view_types.h"
namespace hg {
struct ViewModel; struct UiState; class Canvas;           // forward declarations: screen.h never includes view_model.h
class Screen {
 public:
  virtual ~Screen() = default;
  virtual void bindings(const ViewModel& vm, const UiState& ui, Bindings& out) const = 0;
  virtual void render(Canvas& c, const ViewModel& vm, const UiState& ui, MonoUs now) const = 0;  // content only
  virtual uint16_t framePeriodMs(const ViewModel& vm, const UiState& ui) const = 0;   // 0 = on change only
  virtual uint8_t  sensorDemand(const ViewModel&, const UiState&) const { return 0; } // bit0 mag, bit1 fast SHT
};
}  // namespace hg
