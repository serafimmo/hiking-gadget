#include "build_checks.h"
#include "hal_ports.h"

#include "../config/build_config.h"

namespace hg {

// §4.6 step 1 and §7.3: the TX ring must be sized BEFORE begin(); writes never block (timeout 0). Never wait for
// the host (operator bool is false on battery forever).
void HalSerial::begin() {
  Serial.setTxBufferSize(kSerialTxBufBytes);
  Serial.setTxTimeoutMs(0);
  Serial.begin(115200);
}

size_t HalSerial::read(char* buf, size_t max) {
  size_t n = 0;
  while (n < max && Serial.available() > 0) {
    const int c = Serial.read();
    if (c < 0) break;
    buf[n++] = (char)c;
  }
  return n;
}

size_t HalSerial::write(const char* s, size_t n) { return Serial.write((const uint8_t*)s, n); }

size_t HalSerial::writable() {
  const int a = Serial.availableForWrite();
  return a > 0 ? (size_t)a : 0;
}

bool HalSerial::hostConnected() { return Serial.isConnected(); }

// HWCDC::flush() with timeout 0 discards the queue; give it a bounded 50 ms only when a host reads.
void HalSerial::flushBeforeSleep() {
  if (!Serial.isConnected()) return;
  Serial.setTxTimeoutMs(50);
  Serial.flush();
  Serial.setTxTimeoutMs(0);
}

}  // namespace hg
