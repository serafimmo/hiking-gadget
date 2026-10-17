#include "build_checks.h"
#include "hal_ports.h"

#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>
#include <esp_system.h>

#include "../config/board_pins.h"
#include "../config/build_config.h"

namespace hg {
// ResetReason mirrors esp_reset_reason_t of IDF 5.5 (D36).
static_assert((int)ResetReason::Unknown == ESP_RST_UNKNOWN && (int)ResetReason::PowerOn == ESP_RST_POWERON &&
              (int)ResetReason::Ext == ESP_RST_EXT && (int)ResetReason::Sw == ESP_RST_SW &&
              (int)ResetReason::Panic == ESP_RST_PANIC && (int)ResetReason::IntWdt == ESP_RST_INT_WDT &&
              (int)ResetReason::TaskWdt == ESP_RST_TASK_WDT && (int)ResetReason::Wdt == ESP_RST_WDT &&
              (int)ResetReason::DeepSleep == ESP_RST_DEEPSLEEP && (int)ResetReason::Brownout == ESP_RST_BROWNOUT &&
              (int)ResetReason::Sdio == ESP_RST_SDIO && (int)ResetReason::Usb == ESP_RST_USB &&
              (int)ResetReason::Jtag == ESP_RST_JTAG && (int)ResetReason::Efuse == ESP_RST_EFUSE &&
              (int)ResetReason::PwrGlitch == ESP_RST_PWR_GLITCH && (int)ResetReason::CpuLockup == ESP_RST_CPU_LOCKUP,
              "ResetReason must mirror esp_reset_reason_t");

void HalPower::captureWake(uint8_t levelsAtBoot, uint32_t sampleUs) {
  const esp_reset_reason_t r = esp_reset_reason();
  info_.resetReason = (uint8_t)r <= 15 ? (ResetReason)r : ResetReason::Unknown;
  switch (esp_sleep_get_wakeup_cause()) {   // explicit mapping; anything else -> Other
    case ESP_SLEEP_WAKEUP_UNDEFINED: info_.cause = WakeCause::None; break;
    case ESP_SLEEP_WAKEUP_EXT1: info_.cause = WakeCause::Ext1; break;
    case ESP_SLEEP_WAKEUP_TIMER: info_.cause = WakeCause::Timer; break;
    case ESP_SLEEP_WAKEUP_GPIO: info_.cause = WakeCause::Gpio; break;
    default: info_.cause = WakeCause::Other; break;
  }
  info_.ext1Mask = info_.cause == WakeCause::Ext1 ? esp_sleep_get_ext1_wakeup_status() : 0;
  info_.levelsAtBoot = levelsAtBoot;
  info_.sampleUs = sampleUs;
}

bool HalPower::usbHostRaw() { return HWCDC::isPlugged(); }   // SOF watchdog; flaps, debounced by the app

// §7.4 [V-build]
SleepResult HalPower::lightSleep(uint32_t us, uint8_t excludeKeys) {
  for (uint8_t i = 0; i < kKeyCount; ++i)
    if (!(excludeKeys & (1u << i)) && gpio_get_level((gpio_num_t)kKeyPins[i]) == 0) return SleepResult::Rejected;
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);   // sources persist across sleeps otherwise [HWF F19]
  for (uint8_t i = 0; i < kKeyCount; ++i)
    if (!(excludeKeys & (1u << i))) gpio_wakeup_enable((gpio_num_t)kKeyPins[i], GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup(us < 50000 ? 50000 : us);
  const esp_err_t r = esp_light_sleep_start();
  for (uint8_t i = 0; i < kKeyCount; ++i) {
    if (excludeKeys & (1u << i)) continue;
    gpio_wakeup_disable((gpio_num_t)kKeyPins[i]);
    gpio_set_intr_type((gpio_num_t)kKeyPins[i], GPIO_INTR_DISABLE);   // gpio_wakeup_disable does not restore it [HWF F18]
  }
  if (r == ESP_OK) return SleepResult::Ok;
  if (r == ESP_ERR_SLEEP_REJECT) return SleepResult::Rejected;
  if (r == ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION) return SleepResult::TooShort;
  return SleepResult::Error;
}

// §7.5 [V-build]; returns only when the sleep was rejected
SleepResult HalPower::deepSleep(const DeepSleepPlan& p) {
  if (oled_) oled_->deepOff();
  if (bus0_) bus0_->end();
  if (bus1_) bus1_->end();
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_disable_ext1_wakeup_io(0);                  // clear any previous ext1 pin set [HWF F06]
  for (uint8_t i = 0; i < kKeyCount; ++i) {
    const gpio_num_t pin = (gpio_num_t)kKeyPins[i];
    if (!(p.ext1Mask & (1ULL << kKeyPins[i]))) continue;
    rtc_gpio_pullup_en(pin);                            // RTC pulls are separate from the digital ones [HWF F09]
    rtc_gpio_pulldown_dis(pin);
  }
  if (p.keepRtcPeriph) esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  if (p.ext1Mask) esp_sleep_enable_ext1_wakeup_io(p.ext1Mask, ESP_EXT1_WAKEUP_ANY_LOW);
  if (p.timerUs) esp_sleep_enable_timer_wakeup(p.timerUs);
  if (serial_) serial_->flushBeforeSleep();
  esp_deep_sleep_try_to_start();                        // returns only on rejection
  // Rejected (a wake source was already active, e.g. a key went down): undo what the sleep entry changed. The ext1
  // setup routed the key pads to RTC before the reject, so the digital reads would no longer see the keys.
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
  releaseKeyPins();
  if (bus0_) bus0_->begin();
  if (bus1_) bus1_->begin();
  return SleepResult::Rejected;
}

void HalPower::restart() { esp_restart(); }

uint32_t HalPower::heapFree() const { return ESP.getFreeHeap(); }

}  // namespace hg
