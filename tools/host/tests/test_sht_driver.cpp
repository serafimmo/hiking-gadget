// drivers/sht with FakeI2c + FakeSht (SPEC §6.2): exact bytes, SHT3x only after SHT4x is ruled out, cached checks.
#include "drivers/sht.h"
#include "fakes/fake_sht.h"
#include "tiny_test.h"

using namespace hg;

namespace {
using K = I2cTxn::Kind;
// Ticks the driver every 1 ms until it is done (or limitMs passes); returns the last Poll and the time.
struct Run {
  FakeI2c bus;
  ShtDriver drv{bus};
  int64_t tUs = 1000000;
  ShtSample s{};
  int samples = 0;
  ShtDriver::Poll last = ShtDriver::Poll::Idle;
  void step() { bus.setNow(MonoUs{tUs}); last = drv.tick(MonoUs{tUs}, &s); if (last == ShtDriver::Poll::Sample) ++samples; tUs += 1000; }
  void probe(const ShtProbeHint& h = {}) { bus.setNow(MonoUs{tUs}); drv.beginProbe(MonoUs{tUs}, h); for (int i = 0; i < 3000 && drv.probing(); ++i) step(); }
  ShtDriver::Poll measure(bool heat = false, uint8_t cmd = 0x2F) {
    bus.setNow(MonoUs{tUs});
    if (heat ? !drv.startHeater(MonoUs{tUs}, cmd) : !drv.startMeasure(MonoUs{tUs})) return ShtDriver::Poll::Idle;
    for (int i = 0; i < 3000; ++i) { step(); if (last == ShtDriver::Poll::Sample || last == ShtDriver::Poll::Error) return last; }
    return last;
  }
  bool wrote(size_t i, uint8_t b0, int wn = -1) const {
    const I2cTxn& t = bus.txn(i);
    return t.kind == K::Write && t.w[0] == b0 && (wn < 0 || t.wn == wn);
  }
};
}  // namespace

TEST(sht4x_probe_and_measure_exact_bytes) {
  Run r;
  FakeSht dev(FakeSht::Kind::Sht4x);
  r.bus.attach(0x44, &dev);
  r.probe();
  CHECK_EQ(r.drv.chip(), ShtChip::Sht4x);
  CHECK_EQ(r.drv.addr(), 0x44);
  CHECK_EQ(r.drv.serial(), 0x0F3A21C4u);
  CHECK_EQ(r.bus.txn(0).kind, K::Probe); CHECK_EQ(r.bus.txn(0).addr, 0x44);
  CHECK(r.wrote(1, 0x94, 1));
  CHECK(r.wrote(2, 0x89, 1));
  CHECK_EQ(r.bus.txn(3).kind, K::Read); CHECK_EQ(r.bus.txn(3).rn, 6); CHECK_EQ(r.bus.txn(3).st, I2cStatus::Ok);
  CHECK_EQ(r.bus.txnCount(), (size_t)4);
  r.bus.clearLog();
  CHECK_EQ(r.measure(), ShtDriver::Poll::Sample);
  CHECK(r.wrote(0, 0xFD, 1));
  CHECK_NEAR(r.s.tC, 25.0f, 0.01f);
  CHECK_NEAR(r.s.rh, 56.5f, 0.01f);
  CHECK(!r.s.hot);
  CHECK_EQ(r.bus.txn(1).at.v - r.bus.txn(0).at.v, (int64_t)10000);   // first read at +10 ms
  CHECK(!r.bus.anyWriteStartingWith(0x44, 0x24));
}

TEST(sht3x_detected_only_after_sht4x_ruled_out) {
  Run r;
  FakeSht dev(FakeSht::Kind::Sht3x);
  r.bus.attach(0x44, &dev);
  r.probe();
  CHECK_EQ(r.drv.chip(), ShtChip::Sht3x);
  CHECK(!r.bus.anyWriteStartingWith(0x44, 0x24));          // no 0x24 byte at all during the probe
  // identified <= 100 ms after the address ACK
  const int64_t ackAt = r.bus.txn(0).at.v;
  size_t last = r.bus.txnCount() - 1;
  CHECK(r.wrote(last, 0x30, 2));                            // clear status
  CHECK(r.bus.txn(last).at.v - ackAt <= 100000);
  // the status read F3 2D comes after both 0x89 attempts
  int serialWrites = 0;
  size_t statusAt = 0;
  for (size_t i = 0; i < r.bus.txnCount(); ++i) {
    if (r.wrote(i, 0x89)) ++serialWrites;
    if (r.wrote(i, 0xF3, 2)) statusAt = i;
  }
  CHECK_EQ(serialWrites, 2);
  CHECK(statusAt > 0);
  r.bus.clearLog();
  CHECK_EQ(r.measure(), ShtDriver::Poll::Sample);           // SHT3x measurement: 0x2400, read at +16 ms
  CHECK(r.wrote(0, 0x24, 2)); CHECK_EQ(r.bus.txn(0).w[1], 0x00);
  CHECK_NEAR(r.s.rh, 50.0f, 0.01f);
  CHECK_EQ(r.bus.txn(1).at.v - r.bus.txn(0).at.v, (int64_t)16000);
}

TEST(sht_unknown_none_and_second_address) {
  Run u;
  FakeRegDevice junk;
  u.bus.attach(0x44, &junk);
  u.probe();
  CHECK_EQ(u.drv.chip(), ShtChip::Unknown);
  CHECK_EQ(u.drv.ackAddr(), 0x44);
  CHECK(!u.drv.startMeasure(MonoUs{u.tUs}));

  Run n;
  n.probe();
  CHECK_EQ(n.drv.chip(), ShtChip::None);
  CHECK_EQ(n.bus.txnCount(), (size_t)2);                    // probed 0x44 and 0x45, nothing else
  CHECK_EQ(n.bus.txn(1).addr, 0x45);

  Run b;
  FakeSht dev(FakeSht::Kind::Sht4x);
  b.bus.attach(0x45, &dev);
  b.probe();
  CHECK_EQ(b.drv.chip(), ShtChip::Sht4x);
  CHECK_EQ(b.drv.addr(), 0x45);
}

TEST(sht_busy_wait_honoured_before_the_first_write) {
  Run r;
  FakeSht dev(FakeSht::Kind::Sht4x);
  r.bus.attach(0x44, &dev);
  const int64_t t0 = r.tUs;
  ShtProbeHint h;
  h.busyWaitUs = 700000;
  r.probe(h);
  CHECK_EQ(r.drv.chip(), ShtChip::Sht4x);
  CHECK(r.bus.txn(0).at.v - t0 >= 700000);
}

TEST(sht_cached_chip_checks) {
  Run r;                                                    // cached SHT4x: only 0xFD, and a sample comes back
  FakeSht dev(FakeSht::Kind::Sht4x);
  r.bus.attach(0x44, &dev);
  ShtProbeHint h;
  h.cached = ShtChip::Sht4x; h.addr = 0x44;
  r.probe(h);
  CHECK_EQ(r.drv.chip(), ShtChip::Sht4x);
  CHECK_EQ(r.samples, 1);
  CHECK(r.wrote(0, 0xFD, 1));
  CHECK_EQ(r.bus.txnCount(), (size_t)2);

  Run t;                                                    // cached SHT3x: only F3 2D
  FakeSht d3(FakeSht::Kind::Sht3x);
  t.bus.attach(0x44, &d3);
  h.cached = ShtChip::Sht3x;
  t.probe(h);
  CHECK_EQ(t.drv.chip(), ShtChip::Sht3x);
  CHECK(t.wrote(0, 0xF3, 2));
  CHECK_EQ(t.bus.txnCount(), (size_t)2);

  Run w;                                                    // swapped during sleep: cached SHT3x, now an SHT4x
  FakeSht d4(FakeSht::Kind::Sht4x);
  w.bus.attach(0x44, &d4);
  w.probe(h);
  CHECK_EQ(w.drv.chip(), ShtChip::Sht4x);                   // fell back to the full probe
  CHECK(!d4.sawHeaterViaSht3xCmd);
  CHECK(!w.bus.anyWriteStartingWith(0x44, 0x24));
}

TEST(sht_crc_and_io_errors_counted) {
  Run r;
  FakeSht dev(FakeSht::Kind::Sht4x);
  r.bus.attach(0x44, &dev);
  r.probe();
  dev.corruptCrc = true;
  CHECK_EQ(r.measure(), ShtDriver::Poll::Error);
  CHECK_EQ(r.drv.crcErrors(), 1);
  CHECK_EQ(r.drv.consecutiveErrors(), 1);
  dev.mute = true;
  CHECK_EQ(r.measure(), ShtDriver::Poll::Error);            // write NACK
  dev.mute = false;
  CHECK_EQ(r.drv.ioErrors(), 1);
  CHECK_EQ(r.drv.consecutiveErrors(), 2);
  CHECK_EQ(r.measure(), ShtDriver::Poll::Sample);
  CHECK_EQ(r.drv.consecutiveErrors(), 0);
}

TEST(sht_heater_sample_flagged_hot) {
  Run r;
  FakeSht dev(FakeSht::Kind::Sht4x);
  r.bus.attach(0x44, &dev);
  r.probe();
  r.bus.clearLog();
  const int64_t t0 = r.tUs;
  CHECK_EQ(r.measure(true, 0x2F), ShtDriver::Poll::Sample);
  CHECK(r.s.hot);
  CHECK(r.s.tC > 30.0f);
  CHECK_EQ(dev.lastHeaterCmd, 0x2F);
  CHECK(r.wrote(0, 0x2F, 1));
  CHECK(r.bus.txn(1).at.v - t0 >= 1110000);
  Run s3;
  FakeSht d3(FakeSht::Kind::Sht3x);
  s3.bus.attach(0x44, &d3);
  s3.probe();
  CHECK(!s3.drv.startHeater(MonoUs{s3.tUs}, 0x2F));         // SHT4x only
}
