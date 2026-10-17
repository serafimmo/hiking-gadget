// II2cBus register helpers (drivers/i2c_bus.cpp) and the FakeI2c semantics the driver tests rely on.
#include "fakes/fake_i2c.h"
#include "tiny_test.h"

using namespace hg;

TEST(i2c_helpers_send_exact_bytes) {
  FakeI2c bus;
  FakeRegDevice dev;
  bus.attach(0x0D, &dev);
  CHECK_EQ(bus.writeReg(0x0D, 0x09, 0x1D), I2cStatus::Ok);
  CHECK_EQ(bus.writeCmd(0x0D, 0x0B), I2cStatus::Ok);
  CHECK_EQ(bus.writeCmd16(0x0D, 0x2C06), I2cStatus::Ok);
  CHECK_EQ(bus.txnCount(), (size_t)3);
  CHECK_EQ(bus.txn(0).wn, 2);  CHECK_EQ(bus.txn(0).w[0], 0x09);  CHECK_EQ(bus.txn(0).w[1], 0x1D);
  CHECK_EQ(bus.txn(1).wn, 1);  CHECK_EQ(bus.txn(1).w[0], 0x0B);
  CHECK_EQ(bus.txn(2).wn, 2);  CHECK_EQ(bus.txn(2).w[0], 0x2C);  CHECK_EQ(bus.txn(2).w[1], 0x06);   // MSB first
  CHECK_EQ(dev.regs[0x09], 0x1D);
}

TEST(i2c_readRegs_is_pointer_write_then_repeated_start_read) {
  FakeI2c bus;
  FakeRegDevice dev;
  for (int i = 0; i < 6; ++i) dev.regs[i] = (uint8_t)(0xA0 + i);
  bus.attach(0x1E, &dev);
  uint8_t b[3] = {};
  CHECK_EQ(bus.readRegs(0x1E, 0x02, b, 3), I2cStatus::Ok);
  CHECK_EQ(b[0], 0xA2);  CHECK_EQ(b[2], 0xA4);
  CHECK_EQ(bus.txn(0).kind, I2cTxn::Kind::WriteRead);
  CHECK_EQ(bus.txn(0).wn, 1);  CHECK_EQ(bus.txn(0).w[0], 0x02);  CHECK_EQ(bus.txn(0).rn, 3);
}

TEST(i2c_absent_device_probe_vs_payload) {
  FakeI2c bus;
  CHECK(!bus.probe(0x44));                                    // zero-length probe NACK -> Absent
  CHECK_EQ(bus.txn(0).st, I2cStatus::Absent);
  CHECK_EQ(bus.writeCmd(0x44, 0xFD), I2cStatus::Fail);        // NACK with payload -> 4 on core 3.3.12 -> Fail
  uint8_t b[2];
  CHECK_EQ(bus.read(0x44, b, 2), I2cStatus::Fail);            // short requestFrom -> Fail
  CHECK_EQ(bus.stats().absent, 1u);
  CHECK_EQ(bus.stats().fail, 2u);
  CHECK_EQ(bus.stats().consecutiveFail, 2);
}

TEST(i2c_fault_injection_stuck_and_recover) {
  FakeI2c bus;
  FakeRegDevice dev;
  bus.attach(0x2C, &dev);
  bus.failNext(1);
  CHECK_EQ(bus.writeReg(0x2C, 0x0A, 0x80), I2cStatus::Fail);
  CHECK_EQ(bus.writeReg(0x2C, 0x0A, 0x80), I2cStatus::Ok);
  CHECK_EQ(bus.stats().consecutiveFail, 0);
  bus.setStuck(true);
  CHECK_EQ(bus.writeCmd(0x2C, 0x00), I2cStatus::Stuck);
  CHECK(bus.recover());
  CHECK_EQ(bus.writeCmd(0x2C, 0x00), I2cStatus::Ok);
  CHECK(bus.anyWriteStartingWith(0x2C, 0x0A));
  CHECK(!bus.anyWriteStartingWith(0x2C, 0x24));
  uint8_t found[4];
  CHECK_EQ(bus.scan(found, 4), 1);
  CHECK_EQ(found[0], 0x2C);
}
