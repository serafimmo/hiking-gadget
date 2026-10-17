#include "build_checks.h"
#include "hal_ports.h"

#include <esp_attr.h>

namespace hg {
namespace {
RTC_NOINIT_ATTR RtcState s_rtc;   // survives deep sleep and software resets; garbage after power-on (CRC-checked)
}  // namespace

bool HalKv::begin() {
  ok_ = p_.begin("hg2", false);
  return ok_;
}

bool HalKv::get(const char* key, void* buf, size_t len) {
  if (!ok_ || !p_.isKey(key) || p_.getBytesLength(key) != len) return false;
  return p_.getBytes(key, buf, len) == len;
}

bool HalKv::put(const char* key, const void* buf, size_t len) { return ok_ && p_.putBytes(key, buf, len) == len; }

bool HalKv::erase(const char* key) { return ok_ && (!p_.isKey(key) || p_.remove(key)); }

bool HalKv::eraseAll() { return ok_ && p_.clear(); }

RtcState& HalRtcStore::state() { return s_rtc; }

}  // namespace hg
