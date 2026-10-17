// End-to-end scenarios of AppCore::step() with every port faked (SPEC §10.3 test_app_scenarios).
#include <cstdio>
#include <cstring>

#include "fakes/world.h"
#include "tiny_test.h"
#include "ui/strings.h"

using namespace hg;

namespace {
constexpr uint8_t kUp = 1, kDn = 2, kHash = 4, kStar = 8;
[[maybe_unused]] bool toastIs(World& w, uint16_t id) {
  char t[64];
  w.app->ui().toastText(t, sizeof t);
  const UiState& u = w.app->ui().state();
  for (uint8_t i = 0; i < u.toastCount; ++i) if (u.toastQ[(u.toastHead + i) % 4].str == id) return true;
  (void)t;
  return false;
}
void bootSynced(World& w) {
  w.clk.us = 5 * kUsPerS;
  w.clk.wallValid = true;
  w.clk.wallOffsetUs = 1791736927LL * kUsPerS - w.clk.us;   // Sun 19:42:07 MSK
  w.power.info = WakeInfo{ResetReason::PowerOn, WakeCause::None, 0, 0, 1000};
  memset(&w.rtcStore.s, 0x5A, sizeof w.rtcStore.s);
  w.bootNow();
  w.run(2000);                                              // splash, probes
}
}  // namespace

TEST(app_boot_cold_probes_and_splash) {
  World w;
  bootSynced(w);
  CHECK_EQ(w.app->displayState(), DisplayState::On);
  CHECK_EQ(w.app->ui().topScreen(w.app->viewModel()), ScreenId::Home);
  CHECK_EQ(w.app->viewModel().thermo.chip, ShtChip::Sht4x);
  CHECK(w.app->viewModel().thermo.valid);
  CHECK_NEAR(w.app->viewModel().thermo.tC, 25.0f, 0.05f);
  CHECK_EQ(w.app->viewModel().mag.chip, MagChip::Qmc5883l);
  CHECK(w.out("=== Hiking Gadget 2.0.0"));
  CHECK(w.out("temp SHT4x @44"));
  CHECK(w.display.spans > 0);
}

TEST(app_stopwatch_across_pages_display_off_and_deep_sleep) {
  World w;
  bootSynced(w);
  w.press(kDn);                                             // Stopwatch page
  CHECK_EQ(w.app->ui().state().page, 1);
  const int64_t t0 = w.ms();
  w.press(kStar);                                           // start (on Down)
  CHECK_EQ(w.app->stopwatch().state(), Stopwatch::State::Running);
  w.press(kDn); w.press(kDn);                               // other pages: the model keeps running
  w.run(40000);                                             // display dims and turns off
  CHECK_EQ(w.app->displayState(), DisplayState::Off);
  const int64_t el = w.app->stopwatch().elapsedUs(w.mono.now()) / 1000;
  CHECK(el >= w.ms() - t0 - 400 && el <= w.ms() - t0);
  // manual power off from Home: hold * 1.5 s
  w.press(kUp);                                             // the first key only wakes the display (swallowed)
  CHECK_EQ(w.app->ui().state().page, 3);
  w.press(kDn); w.press(kDn);
  CHECK_EQ(w.app->ui().state().page, 0);
  w.keys.mask = kStar; w.run(1700); w.keys.mask = 0;
  w.run(3000);
  CHECK(w.asleep);
  CHECK_EQ(w.power.lastPlan.ext1Mask, 0x72ull);
  w.sleepUntil(w.ms() + 600000, kHash);                     // 10 min later a key wakes it
  w.run(500);
  const int64_t el2 = w.app->stopwatch().elapsedUs(w.mono.now()) / 1000;
  CHECK_EQ(w.app->stopwatch().state(), Stopwatch::State::Running);
  CHECK(el2 >= w.ms() - t0 - 1500 && el2 <= w.ms() - t0 + 1500);
  CHECK_EQ(w.app->ui().state().page, 0);
}

TEST(app_timer_alert_on_compass_and_while_dark) {
  World w;
  bootSynced(w);
  w.press(kDn); w.press(kDn);                               // Timer
  w.press(kHash); w.press(kHash); w.press(kHash); w.press(kHash); w.press(kHash);   // preset 2 -> 1 (wraps: 3,4,5,0,1)
  CHECK_EQ(w.app->countdown().preset(), 1);                 // 3 min
  w.press(kStar);                                           // start
  CHECK_EQ(w.app->countdown().state(), Countdown::State::Running);
  w.press(kDn);                                             // Compass
  w.run(181000);
  CHECK(w.app->alertActive());
  CHECK_EQ(w.app->ui().topScreen(w.app->viewModel()), ScreenId::Alert);
  CHECK_EQ(w.app->displayState(), DisplayState::On);
  w.press(kUp);                                             // any key dismisses; at carousel level: slide to Timer
  CHECK(!w.app->alertActive());
  CHECK_EQ(w.app->ui().state().page, 2);
  // again, display off this time
  w.press(kStar);                                           // AGAIN (restart same preset)
  CHECK_EQ(w.app->countdown().state(), Countdown::State::Running);
  w.run(60000);
  CHECK_EQ(w.app->displayState(), DisplayState::Off);
  CHECK(w.power.lightSleeps > 0);                           // dark between deadlines
  w.run(125000);
  CHECK(w.app->alertActive());
  CHECK_EQ(w.app->displayState(), DisplayState::On);
}

TEST(app_timer_alert_from_deep_sleep) {
  World w;
  bootSynced(w);
  w.press(kDn); w.press(kDn);
  w.press(kHash);                                           // preset 3: 10 min
  w.press(kStar);
  w.press(kUp); w.press(kUp);                               // Home
  w.keys.mask = kStar; w.run(1700); w.keys.mask = 0;
  w.run(3000);
  CHECK(w.asleep);
  CHECK(w.power.lastPlan.timerUs > 590ULL * 1000000 && w.power.lastPlan.timerUs < 600ULL * 1000000);
  w.sleepUntil(w.ms() + 3600000);                           // the RTC timer wakes it before that
  CHECK_EQ(w.power.info.cause, WakeCause::Timer);
  CHECK_EQ(w.app->displayState(), DisplayState::Off);       // TIMER wake: dark until the alert
  w.run(3000);
  CHECK(w.app->alertActive());
  CHECK_EQ(w.app->displayState(), DisplayState::On);
  CHECK(w.app->countdown().overtimeUs(w.mono.now()) < 2LL * kUsPerS);
}

TEST(app_wake_swallow) {
  World w;
  bootSynced(w);
  w.run(40000);
  CHECK_EQ(w.app->displayState(), DisplayState::Off);
  w.press(kDn);                                             // wakes only
  CHECK_EQ(w.app->displayState(), DisplayState::On);
  CHECK_EQ(w.app->ui().state().page, 0);
  w.press(kDn);
  CHECK_EQ(w.app->ui().state().page, 1);
}

TEST(app_settings_save_to_nvs) {
  World w;
  bootSynced(w);
  w.press(kUp);                                             // Settings cover
  w.press(kStar);                                           // list
  w.press(kDn); w.press(kDn);                               // Screen off
  w.press(kStar);                                           // picker (30 s)
  w.press(kDn);                                             // 1 min
  w.press(kStar);                                           // save
  CHECK_EQ(w.app->settings().get(SettingId::ScreenOff), 3);
  uint8_t v = 0;
  CHECK(w.kv.get("scr", &v, 1));
  CHECK_EQ(v, 3);
  CHECK(toastIs(w, S_T_SAVED));
}

TEST(app_sync_accept_and_reject) {
  World w;
  bootSynced(w);
  w.sendLine("TIME 1791736930.500");
  w.run(100);
  CHECK(w.out("OK TIME utc=2026-10-11T16:42:"));
  CHECK(w.out("q=SYNCED"));
  CHECK(toastIs(w, S_T_TIME_SET_D));
  w.sendLine("TIME 1600000000");                            // 2020: before kMinValid
  w.run(100);
  CHECK(w.out("ERR TIME BEFORE_MIN"));
  CHECK(toastIs(w, S_T_SYNC_REJ));
  w.sendLine("TIME banana");
  w.run(100);
  CHECK(w.out("ERR TIME FORMAT"));
  CHECK(toastIs(w, S_T_CMD_ERR));
  w.sendLine("PING");
  w.run(100);
  CHECK(w.out("PONG HG2 2.0.0"));
}

TEST(app_sht_disappears_card_and_comes_back) {
  World w;
  bootSynced(w);
  w.sht.mute = true;                                        // wire pulled
  w.run(12000);                                             // 3 failed reads at 2 s on Home, then the re-probe finds nothing
  const Health h = w.app->health().get(Device::Sht).h;
  CHECK(h == Health::Fault || h == Health::Absent);
  CHECK(w.app->viewModel().bg.warn);
  CHECK(w.app->ui().state().card == ErrCode::ShtIo || w.app->ui().state().card == ErrCode::ShtNone);   // CHECK HARDWARE
  CHECK(w.out("E-SHT-IO"));
  w.press(kStar);                                           // OK
  w.sht.mute = false;
  bool back = false;
  for (int i = 0; i < 300 && !back; ++i) { w.run(100); back = w.app->health().get(Device::Sht).h == Health::Ok; }
  CHECK(back);                                              // the back-off re-probe found it (5/10/30 s)
  w.run(300);
  CHECK(toastIs(w, S_T_TEMP_BACK));
  CHECK(w.out("SHT4x @0x44"));
  w.run(3000);
  CHECK(w.app->viewModel().thermo.valid);
}

TEST(app_usb_plugged_blocks_light_sleep) {
  World w;
  w.power.usb = true;
  bootSynced(w);
  w.run(60000);
  CHECK_EQ(w.app->displayState(), DisplayState::Off);
  CHECK_EQ(w.power.lightSleeps, 0u);
  w.power.usb = false;
  w.run(10000);
  CHECK(w.power.lightSleeps > 0);
}

TEST(app_mark_target_with_a_1s_hold) {
  World w;
  bootSynced(w);
  w.press(kUp); w.press(kUp);                               // Compass (page 3)
  w.run(1000);                                              // heading valid
  w.keys.mask = kStar; w.run(1000); w.keys.mask = 0; w.run(300);
  CHECK(w.app->viewModel().target.set);
  CHECK(!toastIs(w, S_HINT_HOLD_CLEAR));
  CHECK(toastIs(w, S_T_TARGET));
}

TEST(app_reset_settings_row_opened_with_a_2s_hold_does_nothing) {
  World w;
  bootSynced(w);
  w.press(kUp);
  w.app->ui().stateForTest().listCursor = SettingId::ResetSettings;
  w.keys.mask = kStar; w.run(2000); w.keys.mask = 0; w.run(300);
  CHECK_EQ(w.app->ui().topScreen(w.app->viewModel()), ScreenId::SettingsList);
  CHECK_EQ(w.power.restarts, 0u);
  CHECK(!w.asleep);
}

TEST(app_k3_shorted_low_while_awake) {
  World w;
  bootSynced(w);
  w.keys.mask = kHash;                                      // K3 shorted
  w.run(10500);
  CHECK_EQ(w.app->ui().state().card, ErrCode::KeyStuck);
  w.run(40000);
  CHECK_EQ(w.app->displayState(), DisplayState::Off);       // the stuck key does not hold the display on
  CHECK(w.power.lightSleeps > 0);                           // and does not block light sleep (excluded from wake)
  w.run(31 * 60000);                                        // auto power-off after 30 min dark
  CHECK(w.asleep);
  CHECK_EQ(w.power.lastPlan.ext1Mask & (1ull << 6), 0ull);  // GPIO6 excluded from ext1
  w.keys.mask = 0;
  w.sleepUntil(w.ms() + 5000, kDn);
  CHECK_EQ(w.app->ui().state().card, ErrCode::KeyStuck);   // the at-sleep card at the next boot
}

TEST(app_usb_plugged_while_dark_is_seen) {
  World w;
  bootSynced(w);
  w.run(40000);
  CHECK_EQ(w.app->displayState(), DisplayState::Off);
  const uint32_t before = w.power.lightSleeps;
  w.power.usb = true;                                       // the host only shows up during the probe windows
  w.run(40000);
  CHECK(w.app->viewModel().sys.usbHost);
  const uint32_t mid = w.power.lightSleeps;
  w.run(10000);
  CHECK_EQ(w.power.lightSleeps, mid);                       // light sleep stopped
  CHECK(mid > before);
}

TEST(app_timer_wake_with_corrupt_rtc_state) {
  World w;
  bootSynced(w);
  memset(&w.rtcStore.s, 0x33, sizeof w.rtcStore.s);
  w.power.info = WakeInfo{ResetReason::DeepSleep, WakeCause::Timer, 0, 0, 300000};
  w.bootNow();
  w.run(500);
  CHECK_EQ(w.app->displayState(), DisplayState::On);
  CHECK_EQ(w.app->ui().state().card, ErrCode::RtcLost);
  CHECK_EQ(w.app->ui().state().cardArg, 1);                 // TIMER LOST
  CHECK(w.out("E-RTC-LOST"));
}

TEST(app_picker_save_with_failing_put) {
  World w;
  bootSynced(w);
  w.kv.failPuts = true;
  w.press(kUp); w.press(kStar); w.press(kDn); w.press(kDn); w.press(kStar); w.press(kDn); w.press(kStar);
  CHECK_EQ(w.app->settings().get(SettingId::ScreenOff), 3);   // applied for this session
  CHECK(toastIs(w, S_T_NOT_STORED));
  CHECK(!toastIs(w, S_T_SAVED));
  CHECK_EQ(w.app->health().get(Device::Nvs).code, ErrCode::NvsFail);
  CHECK(w.out("E-NVS put 'scr' failed"));
}

TEST(app_manual_power_off_2s_before_the_timer_ends) {
  World w;
  bootSynced(w);
  w.sendLine("SET scr 6");                                  // screen off: never (the hold must not be a wake press)
  w.run(100);
  w.press(kDn); w.press(kDn);
  for (int i = 0; i < 4; ++i) w.press(kHash);               // preset 0: 1 min
  CHECK_EQ(w.app->countdown().preset(), 0);
  w.press(kStar);
  w.press(kUp); w.press(kUp);                               // Home
  w.run(56500);
  w.keys.mask = kStar; w.run(1600); w.keys.mask = 0;        // power off ~2 s before the end
  w.run(200);
  CHECK(toastIs(w, S_T_OFF_AFTER));
  CHECK(!w.asleep);
  w.run(3000);
  CHECK(w.app->alertActive());
  w.press(kDn);                                             // dismiss -> the deferred power-off runs
  w.run(5000);
  CHECK(w.asleep);
}

TEST(app_plus_one_minute_at_10h) {
  World w;
  bootSynced(w);
  w.press(kDn); w.press(kDn);
  w.press(kHash); w.press(kHash); w.press(kHash);           // preset 5: 60 min
  w.press(kStar);
  for (int i = 0; i < 540; ++i) { w.keys.mask = kHash; w.run(40); w.keys.mask = 0; w.run(40); }
  CHECK_EQ(w.app->countdown().totalUs(), Countdown::kMaxTotalUs);
  w.press(kHash);
  CHECK(toastIs(w, S_T_MAX_10H));
}

TEST(app_boot_approx_clock_and_check_hardware) {
  World w;
  w.bus0.detach(0x44);                                      // no SHT
  w.clk.us = 5 * kUsPerS;
  w.power.info = WakeInfo{ResetReason::PowerOn, WakeCause::None, 0, 0, 1000};
  memset(&w.rtcStore.s, 0x5A, sizeof w.rtcStore.s);
  w.bootNow();                                              // no wall clock, no NVS: the build stamp -> Approx
  w.run(2500);
  CHECK_EQ(w.app->ui().state().card, ErrCode::ShtNone);    // CHECK HARDWARE
  CHECK(toastIs(w, S_T_CLOCK_APPROX));
  CHECK(w.app->viewModel().bg.syncTag);
  const uint8_t pending = w.app->ui().state().toastCount;
  CHECK(pending >= 1);
  w.run(9000);                                              // the card auto-dismisses; the toast is shown after it
  CHECK_EQ(w.app->ui().state().card, ErrCode::None);
  w.run(3000);
  CHECK_EQ(w.app->ui().state().toastCount, 0);
}

TEST(app_headless_without_display) {
  World w;
  w.display.present = false;                                // no panel: models, timer and Serial keep working
  bootSynced(w);
  w.press(kDn);                                             // page keys still work (no frame buffer access)
  w.press(kStar);
  CHECK_EQ(w.app->stopwatch().state(), Stopwatch::State::Running);
  CHECK(w.out("E-OLED-NONE"));
  CHECK_EQ(w.display.spans, 0u);
  w.display.present = true;                                 // the panel answers a retry later
  w.run(1000);
  CHECK(w.display.spans > 0);
}
