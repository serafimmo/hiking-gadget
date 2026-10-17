// UiController navigation (SPEC §2.3-2.5, §10.3 test_ui_nav) driven through the real InputLogic.
#include <cstring>

#include "core/input.h"
#include "fakes/fake_display.h"
#include "fakes/host_u8g2.h"
#include "fixtures.h"
#include "tiny_test.h"
#include "ui/bindings.h"
#include "ui/strings.h"
#include "ui/ui_controller.h"

using namespace hg;

namespace {
constexpr uint8_t kUp = 1, kDn = 2, kHash = 4, kStar = 8;
struct Rig {
  HostU8g2 h;
  Canvas c{&h.g};
  FakeDisplay d{&h.g};
  UiController ui{c, d};
  InputLogic in;
  FixtureCtx f = baseWorld();
  int64_t tMs = 1000;
  bool displayOn = true;
  UiCommand cmds[32];
  int nCmd = 0;
  Rig() { f.vm.settings = &f.settings; ui.render(f.vm, now()); }
  MonoUs now() const { return MonoUs{tMs * 1000}; }
  const UiState& st() const { return ui.state(); }
  void step(uint8_t mask) {
    Bindings b;
    ui.bindings(f.vm, b);
    for (int k = 0; k < kKeyCount; ++k) in.configure((Key)k, keyConfigFor(b.key[k]));
    in.setCaptureAll(b.captureAll);
    in.feed(mask, now());
    InputEvent e;
    while (in.pop(e)) {
      ui.bindings(f.vm, b);   // the bindings of the current layer (the controller uses the captured one)
      const UiCommand cmd = ui.dispatch(e, b, f.vm, now());
      if (cmd.action != ActionId::None && nCmd < 32) cmds[nCmd++] = cmd;
    }
    ui.setHold(in.hold(now()));
    ui.tick(now(), displayOn);
    if (ui.wantsFrame(f.vm, now())) ui.render(f.vm, now());
    tMs += 5;
  }
  void idle(int64_t ms) { for (int64_t end = tMs + ms; tMs < end;) step(0); }
  void press(uint8_t mask, int64_t ms = 60) { for (int64_t end = tMs + ms; tMs < end;) step(mask); idle(250); }
  bool got(ActionId a) const { for (int i = 0; i < nCmd; ++i) if (cmds[i].action == a) return true; return false; }
  ScreenId top() const { return ui.topScreen(f.vm); }
};
}  // namespace

TEST(nav_carousel_wraps_both_ways) {
  Rig r;
  CHECK_EQ(r.st().page, 0);
  r.press(kUp);
  CHECK_EQ(r.st().page, 4);
  r.press(kDn);
  CHECK_EQ(r.st().page, 0);
  for (int i = 0; i < 5; ++i) r.press(kDn);
  CHECK_EQ(r.st().page, 0);
  CHECK_EQ(r.top(), ScreenId::Home);
}

TEST(nav_key_during_a_slide_starts_the_next) {
  Rig r;
  r.press(kDn, 30);                                       // slide 0 -> 1 starts at the Down
  CHECK_EQ(r.st().page, 1);
  r.tMs -= 200;                                           // rewind: the second press lands inside the first slide
  r.f.now = r.now();
  r.press(kDn, 30);
  CHECK_EQ(r.st().page, 2);
}

TEST(nav_home_details_toggle) {
  Rig r;
  r.press(kHash);
  CHECK(r.st().details);
  r.press(kHash);
  CHECK(!r.st().details);
  r.press(kHash);
  r.press(kDn);                                           // leaving the page clears the details flag
  CHECK(!r.st().details);
}

TEST(nav_settings_cover_list_picker_save_cancel) {
  Rig r;
  r.press(kUp);                                           // page 4: Settings cover
  CHECK_EQ(r.top(), ScreenId::SettingsCover);
  r.press(kStar);
  CHECK_EQ(r.top(), ScreenId::SettingsList);
  r.press(kDn); r.press(kDn);
  CHECK_EQ(r.st().listCursor, SettingId::ScreenOff);
  r.press(kStar);
  CHECK_EQ(r.top(), ScreenId::Picker);
  CHECK_EQ(r.st().pending, 2);                            // 30 s
  r.press(kUp);                                           // the item shown above = previous option (20 s)
  CHECK_EQ(r.st().pending, 1);
  r.press(kStar);
  CHECK(r.got(ActionId::SettingSave));
  CHECK_EQ(UiController::savedSetting(r.cmds[r.nCmd - 1]), SettingId::ScreenOff);
  CHECK_EQ(UiController::savedValue(r.cmds[r.nCmd - 1]), 1);
  CHECK_EQ(r.top(), ScreenId::SettingsList);
  r.press(kStar);                                         // reopen, change, cancel
  r.press(kDn);
  CHECK_EQ(r.st().pending, 3);
  r.press(kHash);
  CHECK_EQ(r.top(), ScreenId::SettingsList);
  r.press(kHash);
  CHECK_EQ(r.top(), ScreenId::SettingsCover);
  CHECK_EQ(r.st().depth, 0);
}

TEST(nav_numeric_picker_up_increases) {
  Rig r;
  r.press(kUp); r.press(kStar);
  for (int i = 0; i < 11; ++i) r.press(kDn);              // Declination
  CHECK_EQ(r.st().listCursor, SettingId::Declination);
  r.press(kStar);
  r.press(kUp);
  CHECK_EQ(r.st().pending, 1);
}

TEST(nav_set_time_field_order_commit_and_cancel) {
  Rig r;
  r.press(kUp); r.press(kStar); r.press(kDn);             // Set time
  r.press(kStar);
  CHECK_EQ(r.top(), ScreenId::SetTime);
  CHECK_EQ(r.st().timeField, 0);
  CHECK_EQ(r.st().timeEdit.hour, r.f.vm.clock.local.hour);
  r.press(kUp);
  CHECK_EQ(r.st().timeEdit.hour, (r.f.vm.clock.local.hour + 1) % 24);
  r.press(kStar); r.press(kStar);
  CHECK_EQ(r.st().timeField, 2);
  r.press(kHash);
  CHECK_EQ(r.st().timeField, 1);
  r.press(kHash); r.press(kHash);                         // previous on the first field = cancel
  CHECK_EQ(r.top(), ScreenId::SettingsList);
  CHECK(!r.got(ActionId::SetTimeCommit));
  r.press(kStar);
  for (int i = 0; i < 4; ++i) r.press(kStar);
  CHECK_EQ(r.st().timeField, 4);
  r.press(kStar);
  CHECK(r.got(ActionId::SetTimeCommit));
  CHECK_EQ(r.top(), ScreenId::SettingsList);
}

TEST(nav_modal_depth_two_calibration_back_to_list) {
  Rig r;
  r.press(kUp); r.press(kStar);
  for (int i = 0; i < 14; ++i) r.press(kDn);
  CHECK_EQ(r.st().listCursor, SettingId::Calibrate);
  r.press(kStar);
  CHECK(r.got(ActionId::CmpCalibrate));
  CHECK_EQ(r.top(), ScreenId::Calibration);
  CHECK_EQ(r.st().depth, 2);
  r.press(kHash);                                         // cancel
  CHECK(r.got(ActionId::CalCancel));
  CHECK_EQ(r.top(), ScreenId::SettingsList);
}

TEST(nav_alert_over_picker_returns_with_the_edit) {
  Rig r;
  r.press(kUp); r.press(kStar); r.press(kDn); r.press(kDn); r.press(kStar); r.press(kDn);
  CHECK_EQ(r.st().pending, 3);
  r.f.vm.tm.alertActive = true;
  r.ui.raiseAlert(r.now());
  CHECK_EQ(r.top(), ScreenId::Alert);
  r.ui.dismissAlert(r.now());
  r.f.vm.tm.alertActive = false;
  CHECK_EQ(r.top(), ScreenId::Picker);
  CHECK_EQ(r.st().pending, 3);
}

TEST(nav_alert_at_carousel_slides_to_timer) {
  Rig r;
  r.f.vm.tm.alertActive = true;
  r.ui.raiseAlert(r.now());
  r.ui.dismissAlert(r.now());
  r.f.vm.tm.alertActive = false;
  CHECK_EQ(r.st().page, 2);
  CHECK_EQ(r.top(), ScreenId::Timer);
}

TEST(nav_display_off_reverts_edit_keeps_modal) {
  Rig r;
  r.press(kUp); r.press(kStar); r.press(kDn); r.press(kDn); r.press(kStar); r.press(kDn);
  CHECK_EQ(r.st().pending, 3);
  r.ui.revertEdits();
  CHECK_EQ(r.st().pending, 2);
  CHECK_EQ(r.top(), ScreenId::Picker);
}

TEST(nav_card_queue_order_and_auto_dismiss) {
  Rig r;
  r.ui.showCard(ErrCode::RtcLost, 0);
  r.ui.showCard(ErrCode::KeyStuck, 2);
  r.ui.showCard(ErrCode::ShtNone, 0);
  CHECK_EQ(r.st().card, ErrCode::ShtNone);               // CHECK HARDWARE first
  r.idle(8100);
  CHECK_EQ(r.st().card, ErrCode::KeyStuck);
  r.press(kStar);                                         // OK
  CHECK_EQ(r.st().card, ErrCode::RtcLost);
  r.displayOn = false;
  r.idle(9000);                                           // no auto-dismiss while dark
  CHECK_EQ(r.st().card, ErrCode::RtcLost);
  r.displayOn = true;
  r.idle(8100);
  CHECK_EQ(r.st().card, ErrCode::None);
  r.ui.showCard(ErrCode::I2cStuck, 0);                   // FATAL: stays
  r.idle(10000);
  CHECK_EQ(r.st().card, ErrCode::I2cStuck);
}

TEST(nav_toast_queue_merge_overflow_pause) {
  Rig r;
  r.ui.toast(S_T_SAVED, nullptr, r.now());
  r.ui.toast(S_T_SAVED, nullptr, r.now());                // merged
  CHECK_EQ(r.st().toastCount, 1);
  r.ui.toast(S_T_TIME_SET, nullptr, r.now());
  r.ui.toast(S_T_CLOCK_APPROX, nullptr, r.now());
  r.ui.toast(S_T_CRASH, nullptr, r.now());
  CHECK_EQ(r.st().toastCount, 4);
  r.ui.toast(S_T_SLEEP_FAIL, nullptr, r.now());           // overflow: the oldest pending one is dropped
  CHECK_EQ(r.st().toastCount, 4);
  CHECK_EQ(r.ui.toastsDropped(), 1u);
  r.ui.showCard(ErrCode::ShtNone, 0);                    // paused under a card
  r.idle(5000);
  CHECK_EQ(r.st().toastCount, 4);
  r.press(kStar);                                         // dismiss the card
  r.displayOn = false;
  r.idle(5000);
  CHECK_EQ(r.st().toastCount, 4);                         // paused while dark
  r.displayOn = true;
  r.idle(1450);
  CHECK_EQ(r.st().toastCount, 3);
  r.idle(3 * 1450);
  CHECK_EQ(r.st().toastCount, 0);
}

TEST(nav_captured_binding_D25) {
  Rig r;
  r.press(kUp);                                           // Settings cover
  r.f.settings.set(SettingId::ResetSettings, 0);
  r.ui.stateForTest().listCursor = SettingId::ResetSettings;
  r.press(kStar, 2000);                                   // opens the list on Down; held 2 s on the Reset row
  CHECK_EQ(r.top(), ScreenId::SettingsList);
  CHECK(!r.got(ActionId::ResetSettings));
  CHECK(!r.got(ActionId::PowerOff));
  r.press(kStar, 1700);                                   // a NEW press with the list's hold binding works
  CHECK(r.got(ActionId::ResetSettings));
}

TEST(nav_layer_change_between_down_and_click_drops_the_press) {
  Rig r;
  r.press(kDn);                                           // Stopwatch
  r.f.vm.sw.st = Stopwatch::State::Paused;
  for (int i = 0; i < 6; ++i) r.step(kHash);              // # down (hint on click, reset on hold)
  r.ui.showCard(ErrCode::ShtNone, 0);                    // a card appears before the release
  for (int i = 0; i < 6; ++i) r.step(0);
  CHECK_EQ(r.st().toastCount, 0);                         // no hint toast
  r.press(kStar);                                         // dismiss
  r.press(kHash);                                         // a clean click shows the hint
  CHECK_EQ(r.st().toastCount, 1);
}

TEST(nav_diag_buttons_exit_by_hold) {
  Rig r;
  r.press(kUp); r.press(kStar);
  for (int i = 0; i < 15; ++i) r.press(kDn);
  CHECK_EQ(r.st().listCursor, SettingId::Diagnostics);
  r.press(kStar);
  r.press(kDn); r.press(kDn);
  CHECK_EQ(r.st().diagPage, 2);
  r.press(kUp, 500);                                      // captured: nothing moves
  CHECK_EQ(r.st().diagPage, 2);
  r.press(kHash, 2200);                                   // any key held 2 s exits to page 4
  CHECK_EQ(r.st().diagPage, 3);
  CHECK_EQ(r.top(), ScreenId::Diagnostics);
}
