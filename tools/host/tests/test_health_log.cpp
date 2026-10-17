// core/health (§9.4 table, registry) and core/log_ring.
#include <cstdio>
#include <cstring>

#include "core/health.h"
#include "core/log_ring.h"
#include "tiny_test.h"

using namespace hg;

namespace {
int gSinkCalls = 0;
void sink(void* ctx, Device, const DeviceHealth&, const DeviceHealth&) { ++gSinkCalls; ++*(int*)ctx; }
}  // namespace

TEST(health_error_table) {
  for (int i = 0; i < (int)ErrCode::Count; ++i) {
    const ErrInfo& e = errInfo((ErrCode)i);
    CHECK_EQ((int)e.code, i);
    CHECK(e.mnemonic && strlen(e.mnemonic) >= 2);
  }
  CHECK_STR(errInfo(ErrCode::MagNoStatus).mnemonic, "W-MAG-NOSTATUS");
  CHECK_EQ(errInfo(ErrCode::OledNone).sev, Severity::Fatal);
  CHECK_EQ(errInfo(ErrCode::Sht3xInfo).sev, Severity::Info);
  CHECK_STR(deviceName(Device::Bus1), "I2C1");
}

TEST(health_registry) {
  HealthRegistry r;
  int calls = 0;
  r.setSink(sink, &calls);
  CHECK(!r.anyWarnOrWorse());
  CHECK_EQ(r.worst(), ErrCode::None);
  r.set(Device::Sht, Health::Ok, ErrCode::None, MonoUs{1});
  r.set(Device::Sht, Health::Ok, ErrCode::None, MonoUs{2});    // no change: no log, no revision
  CHECK_EQ(calls, 1); CHECK_EQ(r.revision(), 1u);
  r.set(Device::Mag, Health::Ok, ErrCode::MagUncal, MonoUs{3});   // info only
  CHECK(!r.anyWarnOrWorse());
  r.set(Device::Clock, Health::Degraded, ErrCode::ClockLost, MonoUs{4});
  CHECK(r.anyWarnOrWorse());
  CHECK_EQ(r.worst(), ErrCode::ClockLost);
  r.set(Device::Sht, Health::Absent, ErrCode::ShtNone, MonoUs{5});
  CHECK_EQ(r.worst(), ErrCode::ShtNone);                         // Fault beats Warn
  CHECK_EQ(r.get(Device::Sht).since.v, (int64_t)5);
  r.set(Device::Sht, Health::Ok, ErrCode::None, MonoUs{6});
  r.set(Device::Clock, Health::Ok, ErrCode::None, MonoUs{7});
  CHECK(!r.anyWarnOrWorse());
  CHECK_EQ(calls, 6);
  CHECK_EQ(r.get(Device::Clock).errors, 1u);
}

TEST(log_ring_wraps_and_truncates) {
  LogRing l;
  CHECK_EQ(l.count(), 0);
  CHECK_STR(l.line(0), "");
  char b[32];
  for (int i = 0; i < 50; ++i) { snprintf(b, sizeof b, "line %d", i); l.push(b); }
  CHECK_EQ(l.count(), LogRing::kLines);
  CHECK_STR(l.line(0), "line 2");                               // the two oldest were overwritten
  CHECK_STR(l.line(47), "line 49");
  char big[200];
  memset(big, 'x', sizeof big); big[199] = 0;
  l.push(big);
  CHECK_EQ(strlen(l.line(47)), (size_t)LogRing::kLen - 1);
  l.push(nullptr);
  CHECK_STR(l.line(47), "");
}
