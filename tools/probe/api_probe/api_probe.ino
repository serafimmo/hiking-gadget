// HAL API compile probe (SPEC §13 WP12): every core 3.3.12 / IDF 5.5 call the firmware relies on, in one sketch.
// Build: tools/build.sh probe. It only has to compile; it is never flashed.
#include <Preferences.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include <esp_rtc_time.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <sys/time.h>
void setup() {
  Serial.setTxBufferSize(4096); Serial.setTxTimeoutMs(0); Serial.begin(115200);
  (void)Serial.availableForWrite(); (void)Serial.isConnected(); (void)HWCDC::isPlugged();
  Wire.begin(8, 7, 100000); Wire.setTimeOut(20); Wire1.begin(10, 11, 400000); Wire1.setTimeOut(20);
  u8g2_t u; u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u, U8G2_R0, u8x8_byte_empty, u8x8_dummy_cb);
  u8x8_SetI2CAddress(u8g2_GetU8x8(&u), 0x7A); u8g2_InitDisplay(&u); u8g2_SetFlipMode(&u, 1); u8g2_UpdateDisplayArea(&u, 0, 0, 16, 1);
  gpio_sleep_sel_dis(GPIO_NUM_4); rtc_gpio_hold_dis(GPIO_NUM_4); rtc_gpio_deinit(GPIO_NUM_4);
  rtc_gpio_pullup_en(GPIO_NUM_4); rtc_gpio_pulldown_dis(GPIO_NUM_4);
  gpio_wakeup_enable(GPIO_NUM_4, GPIO_INTR_LOW_LEVEL); esp_sleep_enable_gpio_wakeup(); gpio_wakeup_disable(GPIO_NUM_4);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); esp_sleep_disable_ext1_wakeup_io(0);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_sleep_enable_ext1_wakeup_io(0x72, ESP_EXT1_WAKEUP_ANY_LOW); esp_sleep_enable_timer_wakeup(1000000);
  (void)esp_sleep_get_ext1_wakeup_status(); (void)esp_sleep_get_wakeup_cause(); (void)esp_reset_reason();
  (void)esp_rtc_get_time_us(); (void)esp_timer_get_time();
  struct timeval tv; gettimeofday(&tv, nullptr);
  setCpuFrequencyMhz(80); rgbLedWrite(48, 0, 0, 0); (void)analogReadMilliVolts(2);
  Preferences p; p.begin("probe", false); p.end();
  enableLoopWDT();
  if (esp_light_sleep_start() == ESP_ERR_SLEEP_REJECT) Serial.println("rejected");
  esp_deep_sleep_try_to_start();
}
void loop() {}
