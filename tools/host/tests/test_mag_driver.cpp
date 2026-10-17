// drivers/mag with FakeI2c + FakeMag (SPEC §6.3).
#include "drivers/mag.h"
#include "fakes/fake_mag.h"
#include "tiny_test.h"

using namespace hg;

namespace {
using K = I2cTxn::Kind;
struct Rig {
  FakeI2c bus;
  MagDriver drv{bus};
  int64_t tUs = 1000000;
  MagSample s{};
  int got = 0;
  void run(int64_t ms) {
    for (int64_t end = tUs + ms * 1000; tUs < end; tUs += 1000) {
      bus.setNow(MonoUs{tUs});
      if (drv.poll(MonoUs{tUs}, &s)) ++got;
    }
  }
  void start() { bus.setNow(MonoUs{tUs}); drv.start(MonoUs{tUs}); }
  bool reg(size_t i, uint8_t r, uint8_t v) const { const I2cTxn& t = bus.txn(i); return t.kind == K::Write && t.wn == 2 && t.w[0] == r && t.w[1] == v; }
  int findWrite(uint8_t r, uint8_t v, size_t from = 0) const { for (size_t i = from; i < bus.txnCount(); ++i) if (reg(i, r, v)) return (int)i; return -1; }
};
}  // namespace

TEST(mag_probe_order_and_ids) {
  Rig a;
  FakeMag l(FakeMag::Kind::QmcL);
  a.bus.attach(0x0D, &l);
  CHECK_EQ(a.drv.probe(), MagChip::Qmc5883l);
  CHECK(!a.drv.idWarning());
  CHECK_EQ(a.bus.txn(0).addr, 0x0D);
  CHECK_EQ(a.drv.lsbPerGauss(), 3000.0f);
  CHECK_EQ(a.drv.drdy(), DrdyKind::RisingLevel);

  Rig b;
  FakeMag p(FakeMag::Kind::QmcP);
  b.bus.attach(0x2C, &p);
  CHECK_EQ(b.drv.probe(), MagChip::Qmc5883p);
  CHECK_EQ(b.bus.txn(0).addr, 0x0D); CHECK_EQ(b.bus.txn(1).addr, 0x2C);
  CHECK_EQ(b.drv.drdy(), DrdyKind::None);

  Rig c;
  FakeMag h(FakeMag::Kind::Hmc);
  c.bus.attach(0x1E, &h);
  CHECK_EQ(c.drv.probe(), MagChip::Hmc5883l);
  CHECK_EQ(c.bus.txn(0).addr, 0x0D); CHECK_EQ(c.bus.txn(1).addr, 0x2C); CHECK_EQ(c.bus.txn(2).addr, 0x1E);

  Rig w;                                                    // QMC5883L with ID != 0xFF: assumed, with a warning
  FakeMag l2(FakeMag::Kind::QmcL);
  l2.regs[0x0D] = 0x00;
  w.bus.attach(0x0D, &l2);
  CHECK_EQ(w.drv.probe(), MagChip::Qmc5883l);
  CHECK(w.drv.idWarning());

  Rig u;                                                    // something else at 0x2C
  FakeRegDevice junk;
  junk.regs[0] = 0x42;
  u.bus.attach(0x2C, &junk);
  CHECK_EQ(u.drv.probe(), MagChip::Unknown);
  CHECK_EQ(u.drv.addr(), 0x2C); CHECK_EQ(u.drv.idByte(), 0x42);

  Rig n;
  CHECK_EQ(n.drv.probe(), MagChip::None);
}

TEST(mag_qmcl_init_bytes_status_first_and_decode) {
  Rig r;
  FakeMag l(FakeMag::Kind::QmcL);
  r.bus.attach(0x0D, &l);
  r.drv.probe();
  r.bus.clearLog();
  r.start();
  CHECK(r.reg(0, 0x0A, 0x80));
  r.run(200);
  const int i = r.findWrite(0x0B, 0x01);
  CHECK(i > 0);
  CHECK(r.bus.txn(i).at.v - r.bus.txn(0).at.v >= 10000);   // after the 10 ms reset wait
  CHECK(r.reg(i + 1, 0x0A, 0x00));                          // INT_ENB = 0: DRDY pin enabled
  CHECK(r.reg(i + 2, 0x09, 0x15));
  CHECK_EQ(r.bus.txn(i + 3).kind, K::WriteRead); CHECK_EQ(r.bus.txn(i + 3).w[0], 0x09);   // read-back
  CHECK_EQ(r.bus.txn(i + 4).kind, K::WriteRead); CHECK_EQ(r.bus.txn(i + 4).w[0], 0x0B);
  CHECK(!r.drv.initFailed());
  CHECK(r.got >= 5);
  CHECK_EQ(r.s.x, 1000 + (int16_t)(l.samples & 1)); CHECK_EQ(r.s.y, -500); CHECK_EQ(r.s.z, 3000);
  // status (1 byte at 0x06) always precedes a data burst (6 bytes at 0x00)
  bool order = true;
  for (size_t k = 1; k < r.bus.txnCount(); ++k) {
    const I2cTxn& t = r.bus.txn(k);
    if (t.kind == K::WriteRead && t.w[0] == 0x00 && t.rn == 6) {
      const I2cTxn& p = r.bus.txn(k - 1);
      if (!(p.kind == K::WriteRead && p.w[0] == 0x06 && p.rn == 1)) order = false;
    }
  }
  CHECK(order);
  r.bus.clearLog();
  CHECK(r.drv.standby());
  CHECK(r.reg(0, 0x09, 0x14));
}

TEST(mag_qmcl_readback_mismatch_is_an_init_failure) {
  // A device that ACKs every write but never stores it: the 0x09 / 0x0B read-back does not match.
  struct Deaf : FakeRegDevice {
    I2cStatus onWrite(const uint8_t* d, size_t n, MonoUs) override { if (n) ptr = d[0]; return I2cStatus::Ok; }
  } dev;
  dev.regs[0x0D] = 0xFF;
  Rig r;
  r.bus.attach(0x0D, &dev);
  CHECK_EQ(r.drv.probe(), MagChip::Qmc5883l);
  r.start();
  r.run(50);
  CHECK(r.drv.initFailed());
  CHECK(!r.drv.active());
  CHECK_EQ(r.got, 0);
}

TEST(mag_qmcp_init_bytes_axis_rotation_standby) {
  Rig r;
  FakeMag p(FakeMag::Kind::QmcP);
  p.bx = 120; p.by = -340; p.bz = 560;
  r.bus.attach(0x2C, &p);
  r.drv.probe();
  r.bus.clearLog();
  r.start();
  CHECK(r.reg(0, 0x0B, 0x80));
  r.run(200);
  const int i = r.findWrite(0x0B, 0x00);
  CHECK(i > 0);
  CHECK(r.bus.txn(i).at.v - r.bus.txn(0).at.v >= 5000);
  CHECK(r.reg(i + 1, 0x29, 0x06));
  CHECK(r.reg(i + 2, 0x0B, 0x08));
  CHECK(r.reg(i + 3, 0x0A, 0x00));                          // suspend before Normal
  CHECK(r.reg(i + 4, 0x0A, 0xC5));
  CHECK(r.got >= 5);
  CHECK_EQ(r.s.x, 120 + (int16_t)(p.samples & 1)); CHECK_EQ(r.s.y, -340); CHECK_EQ(r.s.z, 560);
  r.bus.clearLog();
  CHECK(r.drv.standby());
  CHECK(r.reg(0, 0x0A, 0x00));
}

TEST(mag_hmc_init_bytes_xzy_be_decode_overflow_standby) {
  Rig r;
  FakeMag h(FakeMag::Kind::Hmc);
  h.bx = -700; h.by = 250; h.bz = -1200;
  r.bus.attach(0x1E, &h);
  r.drv.probe();
  r.bus.clearLog();
  r.start();
  r.run(400);
  CHECK(r.reg(0, 0x00, 0x70)); CHECK(r.reg(1, 0x01, 0x20)); CHECK(r.reg(2, 0x02, 0x00));
  CHECK(r.got >= 2);
  CHECK_EQ(r.s.x, -700 + (int16_t)(h.samples & 1)); CHECK_EQ(r.s.y, 250); CHECK_EQ(r.s.z, -1200);
  CHECK(!r.s.overflow);
  // no MODE register read in the loop (it sets LOCK)
  bool modeRead = false;
  for (size_t k = 0; k < r.bus.txnCount(); ++k) if (r.bus.txn(k).kind == K::WriteRead && r.bus.txn(k).w[0] == 0x02) modeRead = true;
  CHECK(!modeRead);
  h.overflow = true;
  r.run(200);
  CHECK(r.s.overflow);
  CHECK(r.drv.overflows() >= 1);
  r.bus.clearLog();
  CHECK(r.drv.standby());
  CHECK(r.reg(0, 0x02, 0x02));
}

TEST(mag_stuck_identical_and_no_data) {
  Rig r;
  FakeMag l(FakeMag::Kind::QmcL);
  l.frozen = true;
  r.bus.attach(0x0D, &l);
  r.drv.probe();
  r.start();
  r.run(400);
  CHECK(!r.drv.stuck());
  r.run(400);                                               // 25 identical samples at 50 Hz = 0.5 s
  CHECK(r.drv.stuck());

  Rig q;
  FakeMag l2(FakeMag::Kind::QmcL);
  q.bus.attach(0x0D, &l2);
  q.drv.probe();
  q.start();
  q.run(300);
  CHECK(q.got > 0);
  l2.stopped = true;                                        // no new sample for 500 ms
  q.run(450);
  CHECK(!q.drv.stuck());
  q.run(100);
  CHECK(q.drv.stuck());
}

TEST(mag_dead_status_da5883_D34) {
  Rig r;
  FakeMag h(FakeMag::Kind::Hmc);
  h.deadStatus = true;                                      // RDY never sets, data keep changing
  r.bus.attach(0x1E, &h);
  r.drv.probe();
  r.start();
  r.run(1200);
  CHECK(r.drv.noStatus());
  CHECK(!r.drv.stuck());
  const int before = r.got;
  r.bus.clearLog();
  r.run(670);
  CHECK(r.got - before >= 9 && r.got - before <= 11);       // timed reads every 67 ms
  bool statusRead = false;
  for (size_t k = 0; k < r.bus.txnCount(); ++k)
    if (r.bus.txn(k).kind == K::WriteRead && r.bus.txn(k).w[0] == 0x09) statusRead = true;
  CHECK(!statusRead);
  CHECK_EQ(r.bus.txn(0).w[0], 0x03); CHECK_EQ(r.bus.txn(0).rn, 6);

  Rig f;                                                    // dead status AND frozen data -> E-MAG-STUCK
  FakeMag hf(FakeMag::Kind::Hmc);
  hf.deadStatus = true; hf.frozen = true;
  f.bus.attach(0x1E, &hf);
  f.drv.probe();
  f.start();
  f.run(1200);
  CHECK(!f.drv.noStatus());
  CHECK(f.drv.stuck());
}
