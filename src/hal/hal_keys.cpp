#include "build_checks.h"
#include "hal_ports.h"

#include <driver/gpio.h>
#include <driver/rtc_io.h>

#include "../config/board_pins.h"

namespace hg {

void releaseKeyPins() {
  for (uint8_t p : kKeyPins) {
    const gpio_num_t pin = (gpio_num_t)p;
    rtc_gpio_hold_dis(pin);
    rtc_gpio_deinit(pin);
    pinMode(p, INPUT_PULLUP);
    gpio_sleep_sel_dis(pin);   // keep the pull-up in light sleep (the GPIO wake reads the pad)
  }
}

void HalKeys::begin() {
  for (uint8_t p : kKeyPins) pinMode(p, INPUT_PULLUP);
}

uint8_t HalKeys::rawPressedMask() {
  uint8_t m = 0;
  for (uint8_t i = 0; i < kKeyCount; ++i)
    if (digitalRead(kKeyPins[i]) == LOW) m |= (uint8_t)(1u << i);   // active LOW
  return m;
}

}  // namespace hg
