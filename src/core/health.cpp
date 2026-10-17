#include "health.h"

namespace hg {
namespace {
using E = ErrCode;
using V = Severity;
using D = Device;
// §9.4. Indexed by ErrCode; the static_assert below keeps the order honest.
constexpr ErrInfo kErr[(int)E::Count] = {
    {E::None,            "OK",             V::Info,  D::Power},
    {E::OledNone,        "E-OLED-NONE",    V::Fatal, D::Oled},
    {E::OledIo,          "E-OLED-IO",      V::Warn,  D::Oled},
    {E::ShtNone,         "E-SHT-NONE",     V::Fault, D::Sht},
    {E::ShtUnknown,      "E-SHT-UNK",      V::Fault, D::Sht},
    {E::ShtCrc,          "E-SHT-CRC",      V::Fault, D::Sht},
    {E::ShtIo,           "E-SHT-IO",       V::Fault, D::Sht},
    {E::Sht3xInfo,       "I-SHT-3X",       V::Info,  D::Sht},
    {E::HeatSkipHot,     "W-HEAT-HOT",     V::Warn,  D::Sht},
    {E::MagNone,         "E-MAG-NONE",     V::Fault, D::Mag},
    {E::MagUnknown,      "E-MAG-UNK",      V::Fault, D::Mag},
    {E::MagIdWarn,       "W-MAG-ID",       V::Warn,  D::Mag},
    {E::MagStuck,        "E-MAG-STUCK",    V::Fault, D::Mag},
    {E::MagSat,          "W-MAG-SAT",      V::Warn,  D::Mag},
    {E::MagUncal,        "I-MAG-UNCAL",    V::Info,  D::Mag},
    {E::MagCalOtherChip, "W-MAG-CALCHIP",  V::Warn,  D::Mag},
    {E::MagNoStatus,     "W-MAG-NOSTATUS", V::Warn,  D::Mag},
    {E::I2cStuck,        "E-I2C-STUCK",    V::Fatal, D::Bus0},
    {E::I2cRecovered,    "W-I2C-REC",      V::Warn,  D::Bus0},
    {E::KeyStuck,        "E-KEY-STUCK",    V::Warn,  D::Keys},
    {E::NvsFail,         "E-NVS",          V::Fault, D::Nvs},
    {E::CfgReset,        "W-CFG-RESET",    V::Warn,  D::Nvs},
    {E::ClockLost,       "W-CLK-LOST",     V::Warn,  D::Clock},
    {E::SyncRejected,    "W-SYNC-REJ",     V::Warn,  D::Clock},
    {E::CrashRestored,   "W-RST-CRASH",    V::Warn,  D::Power},
    {E::Brownout,        "W-BROWNOUT",     V::Warn,  D::Power},
    {E::SleepFail,       "E-PWR-SLEEP",    V::Warn,  D::Power},
    {E::RtcLost,         "E-RTC-LOST",     V::Fault, D::Power},
};
constexpr bool tableInOrder() {
  for (int i = 0; i < (int)E::Count; ++i)
    if ((int)kErr[i].code != i) return false;
  return true;
}
static_assert(tableInOrder(), "kErr must be indexed by ErrCode");

bool counts(const DeviceHealth& h) {
  return h.code != E::None && h.h != Health::Ok && kErr[(int)h.code].sev >= V::Warn;
}
}  // namespace

const ErrInfo& errInfo(ErrCode c) { return kErr[(int)c < (int)E::Count ? (int)c : 0]; }

const char* deviceName(Device d) {
  static const char* const kNames[(int)D::Count] = {"OLED", "SHT", "MAG", "I2C0", "I2C1", "KEYS", "NVS", "CLOCK", "POWER"};
  return (int)d < (int)D::Count ? kNames[(int)d] : "?";
}

void HealthRegistry::set(Device d, Health h, ErrCode c, MonoUs now) {
  if ((int)d >= (int)D::Count) return;
  DeviceHealth& cur = d_[(int)d];
  if (cur.h == h && cur.code == c) return;
  const DeviceHealth before = cur;
  if ((h == Health::Fault || h == Health::Degraded) && before.h != h) ++cur.errors;
  cur.h = h;
  cur.code = c;
  cur.since = now;
  ++rev_;
  if (sink_) sink_(ctx_, d, before, cur);
}

const DeviceHealth& HealthRegistry::get(Device d) const { return d_[(int)d < (int)D::Count ? (int)d : 0]; }

bool HealthRegistry::anyWarnOrWorse() const {
  for (const DeviceHealth& h : d_)
    if (counts(h)) return true;
  return false;
}

ErrCode HealthRegistry::worst() const {
  ErrCode w = E::None;
  for (const DeviceHealth& h : d_)
    if (counts(h) && (w == E::None || kErr[(int)h.code].sev > kErr[(int)w].sev)) w = h.code;
  return w;
}

uint32_t HealthRegistry::revision() const { return rev_; }

void HealthRegistry::setSink(Sink fn, void* ctx) { sink_ = fn; ctx_ = ctx; }

}  // namespace hg
