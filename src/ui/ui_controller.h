#pragma once
#include "canvas.h"
#include "display_port.h"
#include "frame_diff.h"
#include "view_model.h"
namespace hg {
class UiController {
 public:
  UiController(Canvas& c, IDisplay& d);
  void bindings(const ViewModel& vm, Bindings& out) const;                 // of the top-most interactive layer
  UiCommand dispatch(const InputEvent& e, const Bindings& b, const ViewModel& vm, MonoUs now);  // UI-only actions handled here
  void raiseAlert(MonoUs now);  void dismissAlert(MonoUs now);
  void toast(uint16_t str, const char* arg, MonoUs now);   // enqueues (§2.3.7); merges an identical consecutive toast
  void showCard(ErrCode c, uint8_t arg = 0);               // queued; arg = key index (KEY STUCK) or variant (STATE LOST)
  void openCalibration();  void openPowerOff(MonoUs now);  void revertEdits();
  bool wantsFrame(const ViewModel& vm, MonoUs now) const;
  void render(const ViewModel& vm, MonoUs now);                            // content (+slide) + chrome into the u8g2 buffer
  bool flush(uint32_t budgetUs, MonoUs now);                               // FrameDiff -> IDisplay::sendPageSpan; true when complete
  void forceFullRefresh();
  uint8_t sensorDemand(const ViewModel& vm) const;
  bool keepAwake() const;  bool animating(MonoUs now) const;  MonoUs nextFrameAt() const;
  const UiState& state() const;  UiState& stateForTest();
  // dispatch() uses the Binding captured at each key's Down for that press's later events (D25);
  // it obtains screens only through screenFor().

  // Additions to the §4.4 contract (implementation notes):
  void tick(MonoUs now, bool displayOn);                   // advances toast and card timers (paused while dark)
  void showSplash(bool on);
  void closePowerOff();
  void setHold(const HoldState& h);                        // from InputLogic::hold(), for the keycap fill
  void closeModal();                                       // pops the top modal (the app closes Calibration)
  bool modalOpen(ScreenId s) const;
  // SettingSave packs the setting and the value: arg = (id << 16) | (uint16_t)value
  static SettingId savedSetting(const UiCommand& c) { return (SettingId)((uint32_t)c.arg >> 16); }
  static int16_t savedValue(const UiCommand& c) { return (int16_t)(uint16_t)(c.arg & 0xFFFF); }
  ScreenId topScreen(const ViewModel& vm) const;           // alert > power off > splash > card > modal > page
  static ScreenId pageScreen(uint8_t page);                // carousel: Home, Stopwatch, Timer, Compass, Settings cover
  static constexpr uint8_t kPages = 5;
  uint32_t toastsDropped() const;
  uint16_t rejectedLastFrame() const;
  const char* toastText(char* buf, size_t n) const;        // the toast on screen now ("" if none)

 private:
  static constexpr uint8_t kCardQ = 6;
  void startSlide(int8_t dir, const ViewModel* vm, MonoUs now);
  void pushModal(ScreenId s);
  void openSetTime(const ViewModel& vm);
  void timeStep(int dir, uint8_t step);
  uint16_t layerKey(const ViewModel& vm) const;
  UiCommand handle(ActionId a, const Binding& b, const InputEvent& e, const ViewModel& vm, MonoUs now);
  bool toastPaused(bool displayOn) const;
  uint32_t vmSum(const ViewModel& vm) const;
  Canvas& c_;
  IDisplay& d_;
  UiState ui_{};
  FrameDiff diff_;
  uint8_t slideOld_[1024] = {};
  uint8_t slideFrom_ = 0;
  ScreenId overlay_ = ScreenId::Count;                     // PowerOff / Splash, Count = none
  MonoUs overlayAt_{0};
  ErrCode cardQ_[kCardQ] = {};
  uint8_t cardArgQ_[kCardQ] = {};
  uint8_t cardN_ = 0;                                      // queued behind ui_.card
  uint32_t cardShownMs_ = 0;
  MonoUs lastTick_{0};
  bool haveTick_ = false, displayOn_ = true;
  uint32_t toastsDropped_ = 0;
  MonoUs lastRender_{0}, nextFrameAt_{0};
  bool dirty_ = true, rendered_ = false;
  uint32_t lastSum_ = 0;
  uint16_t rejected_ = 0;
  bool alertAtCarousel_ = false;
  // press capture (D25), filled by dispatch()
  Binding captured_[kKeyCount] = {};
  ScreenId capturedLayer_[kKeyCount] = {};
  bool capturedValid_[kKeyCount] = {};
  MonoUs pressedAt_[kKeyCount] = {};
  uint16_t capturedKey_[kKeyCount] = {};                   // layer at Down (screen + page + depth + card)
  int16_t savedPending_ = 0;                               // picker value at open (display-off revert)
  Civil savedTimeEdit_{};
  MonoUs downSince_[kKeyCount] = {};                       // Diagnostics > Buttons exit rule
  MonoUs releaseAt_[kKeyCount] = {};                       // pressed keycaps stay filled >= 80 ms
  uint8_t capDown_ = 0;
  MonoUs lastCaptureEvent_{0};
};
}  // namespace hg
