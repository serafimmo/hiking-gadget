#include "registry.h"

namespace hg {
namespace {
class NullScreen final : public Screen {
 public:
  void bindings(const ViewModel&, const UiState&, Bindings& out) const override { out = Bindings{}; }
  void render(Canvas&, const ViewModel&, const UiState&, MonoUs) const override {}
  uint16_t framePeriodMs(const ViewModel&, const UiState&) const override { return 0; }
};
const NullScreen kNull;
uint32_t gNullHits = 0;
}  // namespace

const Screen& screenFor(ScreenId id) {
  if (const Screen* s = screenForA(id)) return *s;
  if (const Screen* s = screenForB(id)) return *s;
  ++gNullHits;
  return kNull;
}

uint32_t nullScreenHits() { return gNullHits; }

}  // namespace hg
