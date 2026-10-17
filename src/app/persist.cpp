#include "persist.h"

#include <string.h>

#include "../core/crc.h"
#include "../core/tz.h"

namespace hg {
namespace {
constexpr uint32_t kCalMagic = 0x31434748;   // 'HGC1'
constexpr uint32_t kClkMagic = 0x314B4748;   // 'HGK1'
constexpr uint8_t kSchemaVer = 1;

struct Writer {
  uint8_t b[64]; size_t n = 0;
  void bytes(const void* p, size_t k) { memcpy(b + n, p, k); n += k; }
  template <typename T> void put(T v) { bytes(&v, sizeof v); }
  void seal() { const uint32_t c = crc32(b, n); put(c); }
};
struct Reader {
  const uint8_t* b; size_t n, off = 0;
  template <typename T> T get() { T v; memcpy(&v, b + off, sizeof v); off += sizeof v; return v; }
  bool crcOk() const { uint32_t c; memcpy(&c, b + n - 4, 4); return c == crc32(b, n - 4); }
};
constexpr size_t kCalLen = 4 + 1 + 1 + 6 * 4 + 1 + 8 + 4;
constexpr size_t kClkLen = 4 + 1 + 8 + 1 + 1 + 8 + 4 + 4;
}  // namespace

bool Persist::put(const char* key, const void* buf, size_t len) {
  if (ok() && kv_->put(key, buf, len)) return true;
  ++putFail_;
  lastFailKey_ = key;
  return false;
}

Persist::LoadResult Persist::loadSettings(Settings& s) {
  LoadResult r{false, SettingId::Count, 0, 0};
  s = defaultSettings();
  if (!ok()) return r;
  uint8_t ver = 0;
  if (!kv_->get("ver", &ver, 1) || ver != kSchemaVer) { ver = kSchemaVer; put("ver", &ver, 1); }   // migration: none yet
  for (int i = 0; i < (int)SettingId::Count; ++i) {
    const SettingId id = (SettingId)i;
    const SettingDef& d = settingDef(id);
    if (!d.nvsKey) continue;
    int16_t v;
    bool present;
    if (id == SettingId::TimeZone) {
      char z[4] = {};
      present = kv_->get(d.nvsKey, z, sizeof z);
      char zs[5] = {};
      memcpy(zs, z, 4);
      v = present ? (int16_t)findTz(zs) : d.def;
    } else if (d.kind == SettingKind::Number) {
      int8_t b = 0;
      present = kv_->get(d.nvsKey, &b, 1);
      v = b;
    } else {
      uint8_t b = 0;
      present = kv_->get(d.nvsKey, &b, 1);
      v = b;
    }
    if (!present) continue;
    if (!isValid(id, v)) {
      if (!r.anyInvalid) { r.anyInvalid = true; r.firstInvalid = id; r.firstOld = v; }
      ++r.invalidCount;
      v = sanitize(id, v);
    }
    s.set(id, v);
  }
  return r;
}

bool Persist::saveSetting(SettingId id, int16_t v) {
  const SettingDef& d = settingDef(id);
  if (!d.nvsKey) return false;
  if (id == SettingId::TimeZone) {
    char z[4] = {};
    const char* tid = kTzTable[sanitize(id, v)].id;
    memcpy(z, tid, strlen(tid) < 4 ? strlen(tid) : 4);
    return put(d.nvsKey, z, sizeof z);
  }
  if (d.kind == SettingKind::Number) { const int8_t b = (int8_t)v; return put(d.nvsKey, &b, 1); }
  const uint8_t b = (uint8_t)v;
  return put(d.nvsKey, &b, 1);
}

bool Persist::resetSettings() {
  if (!ok()) return false;
  bool all = true;
  for (int i = 0; i < (int)SettingId::Count; ++i) {
    const char* k = settingDef((SettingId)i).nvsKey;
    if (k) all = kv_->erase(k) && all;
  }
  return kv_->erase("tmpre") && all;
}

bool Persist::factory() { return ok() && kv_->eraseAll(); }

uint8_t Persist::loadPreset() {
  uint8_t p = 2;
  if (ok() && kv_->get("tmpre", &p, 1) && p < 6) return p;
  return 2;
}

bool Persist::savePreset(uint8_t p) { return put("tmpre", &p, 1); }

bool Persist::loadCal(MagCal& c, int64_t* savedUtcS) {
  uint8_t b[kCalLen];
  if (!ok() || !kv_->get("cal", b, sizeof b)) return false;
  Reader r{b, sizeof b};
  if (!r.crcOk() || r.get<uint32_t>() != kCalMagic || r.get<uint8_t>() != 1) return false;
  c = MagCal{};
  c.chip = r.get<uint8_t>();
  c.offX = r.get<float>(); c.offY = r.get<float>(); c.scaleX = r.get<float>(); c.scaleY = r.get<float>();
  c.rCalCounts = r.get<float>(); c.lsbPerUt = r.get<float>();
  c.qualityPct = r.get<uint8_t>();
  const int64_t t = r.get<int64_t>();
  if (savedUtcS) *savedUtcS = t;
  c.valid = c.scaleX > 0.2f && c.scaleX < 5.0f && c.scaleY > 0.2f && c.scaleY < 5.0f;
  return c.valid;
}

bool Persist::saveCal(const MagCal& c, int64_t savedUtcS) {
  Writer w;
  w.put(kCalMagic); w.put((uint8_t)1); w.put(c.chip);
  w.put(c.offX); w.put(c.offY); w.put(c.scaleX); w.put(c.scaleY); w.put(c.rCalCounts); w.put(c.lsbPerUt);
  w.put(c.qualityPct); w.put(savedUtcS);
  w.seal();
  return put("cal", w.b, w.n);
}

bool Persist::loadClk(int64_t& utcS, ClockMeta& m) {
  uint8_t b[kClkLen];
  if (!ok() || !kv_->get("clk", b, sizeof b)) return false;
  Reader r{b, sizeof b};
  if (!r.crcOk() || r.get<uint32_t>() != kClkMagic || r.get<uint8_t>() != 1) return false;
  utcS = r.get<int64_t>();
  m.q = (ClockQuality)r.get<uint8_t>();
  m.src = (ClockSource)r.get<uint8_t>();
  m.lastSyncUtcS = r.get<int64_t>();
  m.lastSyncDeltaMs = r.get<int32_t>();
  return true;
}

bool Persist::saveClk(int64_t utcS, const ClockMeta& m) {
  Writer w;
  w.put(kClkMagic); w.put((uint8_t)1); w.put(utcS); w.put((uint8_t)m.q); w.put((uint8_t)m.src);
  w.put(m.lastSyncUtcS); w.put(m.lastSyncDeltaMs);
  w.seal();
  return put("clk", w.b, w.n);
}

uint32_t Persist::loadBuildId() {
  uint32_t v = 0;
  return ok() && kv_->get("bld", &v, 4) ? v : 0;
}

bool Persist::saveBuildId(uint32_t id) { return put("bld", &id, 4); }

uint32_t Persist::bumpBoots() {
  uint32_t n = 0;
  if (!ok()) return 0;
  kv_->get("boots", &n, 4);
  ++n;
  put("boots", &n, 4);
  return n;
}

void Persist::pushResetReason(uint8_t r, uint8_t ring[8]) {
  memset(ring, 0xFF, 8);
  if (!ok()) return;
  kv_->get("rsts", ring, 8);
  memmove(ring + 1, ring, 7);
  ring[0] = r;
  put("rsts", ring, 8);
}

}  // namespace hg
