#pragma once
// Fakes of every app port (SPEC §4.4 app/ports.h) sharing one simulated world time.
#include <cstring>
#include <string>

#include "app/ports.h"

namespace hg {
struct WorldClock {
  int64_t us = 0;            // real time since the simulation started
  int64_t bootAt = 0;        // esp_timer restarts at ~0 after every reset
  int64_t wallOffsetUs = 0;  // wall = us + offset (valid only after a set)
  bool wallValid = false;
};
struct FakeMono : IMonoClock { WorldClock* w; explicit FakeMono(WorldClock* c) : w(c) {} MonoUs now() const override { return MonoUs{w->us - w->bootAt}; } };
struct FakeRtcClock : IRtcClock { WorldClock* w; uint64_t base = 7000000; explicit FakeRtcClock(WorldClock* c) : w(c) {} RtcUs now() const override { return RtcUs{base + (uint64_t)w->us}; } };
struct FakeWall : IWallClock {
  WorldClock* w; explicit FakeWall(WorldClock* c) : w(c) {}
  bool get(UtcMs* out) const override { if (!w->wallValid) { out->v = 0; return false; } out->v = (w->us + w->wallOffsetUs) / 1000; return true; }
  bool set(UtcMs t) override { w->wallOffsetUs = t.v * 1000 - w->us; w->wallValid = true; return true; }
};
struct FakeKeys : IKeys { uint8_t mask = 0; uint8_t rawPressedMask() override { return mask; } };
struct FakeKv : IKvStore {
  struct Entry { char key[16]; uint8_t data[64]; size_t len; bool used; };
  Entry e[40] = {};
  bool present = true, failPuts = false;
  uint32_t puts = 0;
  bool ok() const override { return present; }
  Entry* find(const char* k) { for (auto& x : e) if (x.used && strcmp(x.key, k) == 0) return &x; return nullptr; }
  bool get(const char* k, void* buf, size_t len) override { Entry* x = find(k); if (!present || !x || x->len != len) return false; memcpy(buf, x->data, len); return true; }
  bool put(const char* k, const void* buf, size_t len) override {
    if (!present || failPuts || len > sizeof e[0].data) return false;
    ++puts;
    Entry* x = find(k);
    if (!x) for (auto& y : e) if (!y.used) { x = &y; break; }
    if (!x) return false;
    strncpy(x->key, k, sizeof x->key - 1); memcpy(x->data, buf, len); x->len = len; x->used = true;
    return true;
  }
  bool erase(const char* k) override { Entry* x = find(k); if (x) x->used = false; return present; }
  bool eraseAll() override { for (auto& x : e) x.used = false; return present; }
};
struct FakeRtcStore : IRtcStore { RtcState s{}; FakeRtcStore() { memset(&s, 0xA5, sizeof s); } RtcState& state() override { return s; } };
struct FakeSerial : ISerialPort {
  size_t ring = 256;              // HWCDC TX ring model: short writes when full
  std::string tx, in;             // tx: bytes accepted (the host drains them into 'out'); in: host -> device
  std::string out;
  bool connected = true;
  uint32_t flushes = 0;
  size_t read(char* buf, size_t max) override { const size_t n = in.size() < max ? in.size() : max; memcpy(buf, in.data(), n); in.erase(0, n); return n; }
  size_t write(const char* s, size_t n) override { const size_t free = ring - tx.size(); const size_t w = n < free ? n : free; tx.append(s, w); return w; }
  size_t writable() override { return ring - tx.size(); }
  bool hostConnected() override { return connected; }
  void flushBeforeSleep() override { ++flushes; }
  void drain() { if (connected) out += tx; tx.clear(); }   // the host reads everything that was queued
};
struct FakePower : IPower {
  WorldClock* clk = nullptr;
  int64_t lastWakeUs = -1000000000;   // the last light-sleep wake
  WakeInfo info{ResetReason::PowerOn, WakeCause::None, 0, 0, 1000};
  bool usb = false;
  uint32_t restarts = 0, lightSleeps = 0, deepSleeps = 0;
  SleepResult nextDeep = SleepResult::Ok;
  DeepSleepPlan lastPlan{};
  uint32_t lastLightUs = 0;
  WakeInfo wakeInfo() const override { return info; }
  bool usbHostRaw() override { return usb && (!clk || clk->us - lastWakeUs >= 150000); }
  SleepResult lightSleep(uint32_t us, uint8_t) override { ++lightSleeps; lastLightUs = us; return SleepResult::Ok; }
  SleepResult deepSleep(const DeepSleepPlan& p) override { ++deepSleeps; lastPlan = p; return nextDeep; }
  void restart() override { ++restarts; }
  uint32_t heapFree() const override { return 271000; }
};
struct FakeBattery : IBattery { BatteryState read() override { return BatteryState{false, 0, 0}; } };
struct FakeDrdy : IDrdy { void enable(DrdyKind) override {} void disable() override {} uint32_t edges() const override { return 0; } };
}  // namespace hg
