#pragma once
#include "types.h"
namespace hg {
enum class Device : uint8_t { Oled, Sht, Mag, Bus0, Bus1, Keys, Nvs, Clock, Power, Count };
struct DeviceHealth { Health h; ErrCode code; uint32_t errors; MonoUs since; };
struct ErrInfo { ErrCode code; const char* mnemonic; Severity sev; Device dev; };   // texts: ui errTitle()/errHint()
const ErrInfo& errInfo(ErrCode c);
class HealthRegistry {
 public:
  void set(Device d, Health h, ErrCode c, MonoUs now);   // logs on change
  const DeviceHealth& get(Device d) const;
  bool anyWarnOrWorse() const;  ErrCode worst() const;  uint32_t revision() const;
  // "logs on change": the app installs a sink (core has no logger); called after every change of h or code.
  using Sink = void (*)(void* ctx, Device d, const DeviceHealth& before, const DeviceHealth& after);
  void setSink(Sink fn, void* ctx);

 private:
  DeviceHealth d_[(int)Device::Count] = {};
  uint32_t rev_ = 0;
  Sink sink_ = nullptr;
  void* ctx_ = nullptr;
};
const char* deviceName(Device d);   // "OLED", "SHT", ... (log tags and Diagnostics)
}  // namespace hg
