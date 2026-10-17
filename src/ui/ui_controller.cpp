#include "ui_controller.h"

#include <stdio.h>
#include <string.h>

#include "../config/timing.h"
#include "../core/crc.h"
#include "compositor.h"
#include "../core/tz.h"
#include "screens/registry.h"
#include "screens/screens.h"
#include "strings.h"
#include "theme.h"
#include "widgets.h"

namespace hg {
namespace {
constexpr int64_t ms(uint32_t v) { return (int64_t)v * kUsPerMs; }
constexpr uint32_t kToastTotalMs = kToastSlideMs + kToastMs + kToastSlideMs;

// Card order (§2.5): CHECK HARDWARE, (FATAL) I2C STUCK, KEY STUCK, STATE LOST, SETTINGS RESET, SHT3x INFO.
uint8_t cardRank(ErrCode c) {
  switch (c) {
    case ErrCode::I2cStuck:  return 1;
    case ErrCode::KeyStuck:  return 2;
    case ErrCode::RtcLost:
    case ErrCode::CrashRestored: return 3;
    case ErrCode::CfgReset:  return 4;
    case ErrCode::Sht3xInfo: return 5;
    default:                 return 0;   // any other fault: the CHECK HARDWARE boot report
  }
}
}  // namespace

UiController::UiController(Canvas& c, IDisplay& d) : c_(c), d_(d) {
  ui_.card = ErrCode::None;
  ui_.animations = true;
  ui_.listCursor = SettingId::TimeZone;
}

ScreenId UiController::pageScreen(uint8_t page) {
  static const ScreenId k[kPages] = {ScreenId::Home, ScreenId::Stopwatch, ScreenId::Timer, ScreenId::Compass,
                                     ScreenId::SettingsCover};
  return k[page % kPages];
}

ScreenId UiController::topScreen(const ViewModel& vm) const {
  if (vm.tm.alertActive) return ScreenId::Alert;
  if (overlay_ != ScreenId::Count) return overlay_;
  if (ui_.card != ErrCode::None) return ScreenId::Card;
  if (ui_.depth) return ui_.modal[ui_.depth - 1];
  return pageScreen(ui_.page);
}

void UiController::bindings(const ViewModel& vm, Bindings& out) const {
  out = Bindings{};
  screenFor(topScreen(vm)).bindings(vm, ui_, out);
}

uint16_t UiController::layerKey(const ViewModel& vm) const {
  return (uint16_t)((uint16_t)topScreen(vm) | (ui_.page << 5) | (ui_.depth << 8) | ((uint16_t)ui_.card << 10));
}

void UiController::pushModal(ScreenId s) {
  if (ui_.depth < 2) ui_.modal[ui_.depth++] = s;
  dirty_ = true;
}

void UiController::closeModal() {
  if (ui_.depth) --ui_.depth;
  dirty_ = true;
}

bool UiController::modalOpen(ScreenId s) const {
  for (uint8_t i = 0; i < ui_.depth; ++i) if (ui_.modal[i] == s) return true;
  return false;
}

void UiController::setHold(const HoldState& h) {
  if (h.active != ui_.hold.active || h.key != ui_.hold.key || (int)(h.progress * 8) != (int)(ui_.hold.progress * 8)) dirty_ = true;
  ui_.hold = h;
}

void UiController::closePowerOff() {
  if (overlay_ == ScreenId::PowerOff) overlay_ = ScreenId::Count;
  dirty_ = true;
}

// Initial fields: the current local time if the clock is set, else the build date at 12:00 (§2.5).
void UiController::openSetTime(const ViewModel& vm) {
  Civil c{};
  if (vm.clock.valid) {
    c = vm.clock.local;
  } else {
    char d[12] = {}, t[9] = "12:00:00";
    if (vm.sys.build && strlen(vm.sys.build) >= 11) memcpy(d, vm.sys.build, 11);
    if (!parseBuildStamp(d, t, c)) { c = civilFromEpoch(1791720000); c.hour = 12; c.minute = 0; }
    c.hour = 12; c.minute = 0;
  }
  c.second = 0;
  ui_.timeEdit = c;
  ui_.timeField = 0;
  savedTimeEdit_ = c;
  pushModal(ScreenId::SetTime);
}

// HH, MM wrap inside their range; DD wraps inside the month; MO wraps; YYYY clamps 2025..2099. DD is clamped to
// the month length after MO/YYYY changes.
void UiController::timeStep(int dir, uint8_t step) {
  Civil& c = ui_.timeEdit;
  const int s = dir * (step ? step : 1);
  auto wrap = [](int v, int lo, int hi) { const int n = hi - lo + 1; return lo + ((v - lo) % n + n) % n; };
  switch (ui_.timeField) {
    case 0: c.hour = (uint8_t)wrap(c.hour + s, 0, 23); break;
    case 1: c.minute = (uint8_t)wrap(c.minute + s, 0, 59); break;
    case 2: c.day = (uint8_t)wrap(c.day + s, 1, daysInMonth(c.year, c.month)); break;
    case 3: c.month = (uint8_t)wrap(c.month + s, 1, 12); break;
    default: { int y = c.year + s; c.year = (int16_t)(y < 2025 ? 2025 : y > 2099 ? 2099 : y); break; }
  }
  const uint8_t dim = daysInMonth(c.year, c.month);
  if (c.day > dim) c.day = dim;
  c.weekday = weekdayFromDays(daysFromCivil(c.year, c.month, c.day));
  c.second = 0;
  dirty_ = true;
}

UiCommand UiController::handle(ActionId a, const Binding& b, const InputEvent& e, const ViewModel& vm, MonoUs now) {
  UiCommand cmd{ActionId::None, 0, e.tDown};
  const uint8_t step = e.step ? e.step : 1;
  dirty_ = true;
  switch (a) {
    case ActionId::None: break;
    case ActionId::PagePrev:
    case ActionId::PageNext: {
      const int8_t dir = a == ActionId::PageNext ? +1 : -1;
      startSlide(dir, &vm, now);
      ui_.page = (uint8_t)((ui_.page + kPages + dir) % kPages);
      ui_.details = false;
      break;
    }
    case ActionId::DetailsToggle: ui_.details = !ui_.details; break;
    case ActionId::OpenList:
      ui_.listTop = listScroll(ui_.listTop, ui_.listCursor, 0);
      pushModal(ScreenId::SettingsList);
      break;
    case ActionId::ListUp:
    case ActionId::ListDown: {
      const int dir = a == ActionId::ListDown ? +1 : -1;
      const int c = (int)ui_.listCursor + dir;
      if (c >= 0 && c < (int)SettingId::Count) ui_.listCursor = (SettingId)c;
      ui_.listTop = listScroll(ui_.listTop, ui_.listCursor, dir);
      break;
    }
    case ActionId::ListBack: closeModal(); break;
    case ActionId::ListActivate: {
      const SettingId id = ui_.listCursor;
      switch (settingDef(id).kind) {
        case SettingKind::Enum:
        case SettingKind::Number:
          if (!listRowEditable(id, vm.fmt)) break;
          ui_.pending = vm.settings ? vm.settings->get(id) : settingDef(id).def;
          savedPending_ = ui_.pending;
          pushModal(ScreenId::Picker);
          break;
        case SettingKind::Action:
          if (id == SettingId::SetTime) openSetTime(vm);
          else if (id == SettingId::Diagnostics) { ui_.diagPage = 0; pushModal(ScreenId::Diagnostics); }
          else if (id == SettingId::Calibrate) { pushModal(ScreenId::Calibration); cmd.action = ActionId::CmpCalibrate; }
          break;
        case SettingKind::ActionHold:
          cmd.action = id == SettingId::PowerOff ? ActionId::PowerOff : ActionId::ResetSettings;
          break;
        default: break;
      }
      break;
    }
    case ActionId::PickUp:
    case ActionId::PickDown: {
      // ^ always selects the item shown above: numbers show the larger value above, enums the previous option.
      const SettingId id = ui_.listCursor;
      const bool number = settingDef(id).kind == SettingKind::Number;
      const int dir = (a == ActionId::PickUp) == number ? +1 : -1;
      ui_.pending = stepValue(id, ui_.pending, dir, step);
      break;
    }
    case ActionId::PickCancel: ui_.pending = savedPending_; closeModal(); break;
    case ActionId::SettingSave:
      cmd.action = ActionId::SettingSave;
      cmd.arg = (int32_t)(((uint32_t)ui_.listCursor << 16) | (uint16_t)ui_.pending);
      closeModal();
      break;
    case ActionId::TimeInc: timeStep(+1, step); break;
    case ActionId::TimeDec: timeStep(-1, step); break;
    case ActionId::TimeFieldPrev:
      if (ui_.timeField == 0) closeModal();
      else --ui_.timeField;
      break;
    case ActionId::TimeFieldNext: if (ui_.timeField < 4) ++ui_.timeField; break;
    case ActionId::SetTimeCommit: cmd.action = ActionId::SetTimeCommit; closeModal(); break;
    case ActionId::DiagPrev: ui_.diagPage = (uint8_t)((ui_.diagPage + 4) % 5); break;
    case ActionId::DiagNext: ui_.diagPage = (uint8_t)((ui_.diagPage + 1) % 5); break;
    case ActionId::DiagBack: closeModal(); break;
    case ActionId::CardDismiss:
      ui_.card = ErrCode::None;
      if (cardN_) {
        ui_.card = cardQ_[0];
        ui_.cardArg = cardArgQ_[0];
        for (uint8_t i = 1; i < cardN_; ++i) { cardQ_[i - 1] = cardQ_[i]; cardArgQ_[i - 1] = cardArgQ_[i]; }
        --cardN_;
      }
      cardShownMs_ = 0;
      break;
    case ActionId::ShowHint:
      if (b.hintStr) toast(b.hintStr, nullptr, now);
      break;
    case ActionId::CmpCalibrate: pushModal(ScreenId::Calibration); cmd.action = a; break;
    case ActionId::CalCancel:
    case ActionId::CalDone:
      if (ui_.depth && ui_.modal[ui_.depth - 1] == ScreenId::Calibration) closeModal();
      cmd.action = a;
      break;
    default:   // everything else is an app action (models, power, sensors)
      cmd.action = a;
      cmd.arg = e.step;
      break;
  }
  return cmd;
}

UiCommand UiController::dispatch(const InputEvent& e, const Bindings& b, const ViewModel& vm, MonoUs now) {
  const UiCommand none{ActionId::None, 0, e.tDown};
  const uint8_t k = (uint8_t)e.key & 3;
  if (e.type == InputEvent::Type::Stuck || e.type == InputEvent::Type::Unstuck) return none;
  if (b.captureAll) {   // Diagnostics > Buttons: count and light, nothing else; the exit rule runs in tick()
    lastCaptureEvent_ = now;
    if (e.type == InputEvent::Type::Down) { capDown_ |= (uint8_t)(1u << k); downSince_[k] = e.tDown; }
    if (e.type == InputEvent::Type::Up) capDown_ &= (uint8_t)~(1u << k);
    dirty_ = true;
    return none;
  }
  switch (e.type) {
    case InputEvent::Type::Down:
      captured_[k] = b.key[k];
      capturedKey_[k] = layerKey(vm);
      capturedValid_[k] = true;
      pressedAt_[k] = e.tDown;
      if (b.key[k].icon != IconId::None) ui_.pressedMask |= (uint8_t)(1u << k);
      dirty_ = true;
      return none;
    case InputEvent::Type::Up:
      capturedValid_[k] = false;
      if (now - pressedAt_[k] >= ms(kPressedMinMs)) ui_.pressedMask &= (uint8_t)~(1u << k);
      else releaseAt_[k] = pressedAt_[k] + ms(kPressedMinMs);   // tick() clears it
      if (ui_.hold.active && ui_.hold.key == e.key) ui_.hold.active = false;
      dirty_ = true;
      return none;
    default:
      break;
  }
  if (!capturedValid_[k]) return none;
  // D25: the press acts through the binding it had at Down; if the layer changed since, the press is dropped.
  if (capturedKey_[k] != layerKey(vm)) {
    capturedValid_[k] = false;
    ui_.pressedMask &= (uint8_t)~(1u << k);
    return none;
  }
  const Binding& cb = captured_[k];
  ActionId a = ActionId::None;
  if (e.type == InputEvent::Type::Short) a = cb.shortAct;
  else if (e.type == InputEvent::Type::Long) a = cb.longAct;
  else if (e.type == InputEvent::Type::Repeat && cb.repeat) a = cb.shortAct;
  return handle(a, cb, e, vm, now);
}

void UiController::raiseAlert(MonoUs) {
  alertAtCarousel_ = ui_.depth == 0 && overlay_ == ScreenId::Count;
  dirty_ = true;
}

void UiController::dismissAlert(MonoUs now) {
  if (alertAtCarousel_ && ui_.card == ErrCode::None && ui_.depth == 0 && ui_.page != 2) {
    startSlide(ui_.page < 2 ? +1 : -1, nullptr, now);
    ui_.page = 2;   // Timer (Done)
  }
  dirty_ = true;
}

void UiController::toast(uint16_t s, const char* arg, MonoUs) {
  Toast t{};
  t.str = s;
  if (arg) { strncpy(t.arg, arg, sizeof t.arg - 1); t.arg[sizeof t.arg - 1] = 0; }
  if (ui_.toastCount) {   // merge an identical consecutive toast
    const Toast& last = ui_.toastQ[(ui_.toastHead + ui_.toastCount - 1) % kToastQueue];
    if (last.str == t.str && strcmp(last.arg, t.arg) == 0) return;
  }
  if (ui_.toastCount == kToastQueue) {   // drop the oldest PENDING toast (the head is on screen)
    for (uint8_t i = 1; i + 1 < kToastQueue; ++i)
      ui_.toastQ[(ui_.toastHead + i) % kToastQueue] = ui_.toastQ[(ui_.toastHead + i + 1) % kToastQueue];
    --ui_.toastCount;
    ++toastsDropped_;
  }
  ui_.toastQ[(ui_.toastHead + ui_.toastCount) % kToastQueue] = t;
  if (ui_.toastCount++ == 0) ui_.toastShownMs = 0;
  dirty_ = true;
}

void UiController::showCard(ErrCode c, uint8_t arg) {
  // A head card that has not been on screen yet (boot: the splash is still up) keeps the §2.5 order too.
  if (ui_.card != ErrCode::None && cardShownMs_ == 0 && cardRank(c) < cardRank(ui_.card) && cardN_ < kCardQ) {
    for (uint8_t i = cardN_; i > 0; --i) { cardQ_[i] = cardQ_[i - 1]; cardArgQ_[i] = cardArgQ_[i - 1]; }
    cardQ_[0] = ui_.card;
    cardArgQ_[0] = ui_.cardArg;
    ++cardN_;
    ui_.card = c;
    ui_.cardArg = arg;
    dirty_ = true;
    return;
  }
  if (ui_.card == ErrCode::None) {
    ui_.card = c;
    ui_.cardArg = arg;
    cardShownMs_ = 0;
    dirty_ = true;
    return;
  }
  if (cardRank(ui_.card) == cardRank(c)) { ui_.cardArg = arg; return; }   // same card: refresh its variant
  for (uint8_t i = 0; i < cardN_; ++i)
    if (cardRank(cardQ_[i]) == cardRank(c)) { cardArgQ_[i] = arg; return; }
  if (cardN_ == kCardQ) return;
  uint8_t pos = cardN_;
  while (pos > 0 && cardRank(cardQ_[pos - 1]) > cardRank(c)) {   // keep the §2.5 order
    cardQ_[pos] = cardQ_[pos - 1];
    cardArgQ_[pos] = cardArgQ_[pos - 1];
    --pos;
  }
  cardQ_[pos] = c;
  cardArgQ_[pos] = arg;
  ++cardN_;
}

void UiController::openCalibration() {
  if (ui_.depth < 2) ui_.modal[ui_.depth++] = ScreenId::Calibration;
  dirty_ = true;
}

void UiController::openPowerOff(MonoUs now) {
  overlay_ = ScreenId::PowerOff;
  overlayAt_ = now;
  dirty_ = true;
}

void UiController::showSplash(bool on) {
  if (on) overlay_ = ScreenId::Splash;
  else if (overlay_ == ScreenId::Splash) overlay_ = ScreenId::Count;
  dirty_ = true;
}

// Display off: an uncommitted picker or set-time edit is reverted, the modal stays (§2.3.6).
void UiController::revertEdits() {
  if (!ui_.depth) return;
  if (ui_.modal[ui_.depth - 1] == ScreenId::Picker) ui_.pending = savedPending_;
  if (ui_.modal[ui_.depth - 1] == ScreenId::SetTime) { ui_.timeEdit = savedTimeEdit_; ui_.timeField = 0; }
  dirty_ = true;
}

void UiController::startSlide(int8_t dir, const ViewModel* vm, MonoUs now) {
  if (ui_.sliding) {                                   // a nav key during a slide jumps it to its end first
    ui_.sliding = false;
    if (vm) render(*vm, now);                          // the frozen frame is the finished page, not a mid-slide mix
  }
  if (c_.buffer()) memcpy(slideOld_, c_.buffer(), sizeof slideOld_);
  slideFrom_ = ui_.page;
  ui_.sliding = ui_.animations;
  ui_.slideDir = dir;
  ui_.slideAt = now;
  dirty_ = true;
}

bool UiController::toastPaused(bool displayOn) const {
  return !displayOn || ui_.card != ErrCode::None || overlay_ != ScreenId::Count;
}

void UiController::tick(MonoUs now, bool displayOn) {
  for (uint8_t k = 0; k < kKeyCount; ++k)
    if (releaseAt_[k].v && now.v >= releaseAt_[k].v && !capturedValid_[k]) {
      ui_.pressedMask &= (uint8_t)~(1u << k);
      releaseAt_[k] = MonoUs{0};
      dirty_ = true;
    }
  const int64_t dt = haveTick_ && now.v > lastTick_.v ? now - lastTick_ : 0;
  const uint32_t dms = (uint32_t)(dt / kUsPerMs);
  lastTick_ = now;
  haveTick_ = true;
  displayOn_ = displayOn;
  if (ui_.toastCount && !toastPaused(displayOn)) {
    ui_.toastShownMs = (uint16_t)(ui_.toastShownMs + dms > 60000 ? 60000 : ui_.toastShownMs + dms);
    const uint32_t total = ui_.animations ? kToastTotalMs : kToastMs;
    if (ui_.toastShownMs >= total) {
      ui_.toastHead = (uint8_t)((ui_.toastHead + 1) % kToastQueue);
      --ui_.toastCount;
      ui_.toastShownMs = 0;
      dirty_ = true;
    }
  }
  if (ui_.card != ErrCode::None && displayOn) {
    cardShownMs_ += dms;
    if (ui_.card != ErrCode::I2cStuck && cardShownMs_ >= kCardAutoDismissMs) {   // FATAL cards stay
      ui_.card = ErrCode::None;
      if (cardN_) {
        ui_.card = cardQ_[0];
        ui_.cardArg = cardArgQ_[0];
        for (uint8_t i = 1; i < cardN_; ++i) { cardQ_[i - 1] = cardQ_[i]; cardArgQ_[i - 1] = cardArgQ_[i]; }
        --cardN_;
      }
      cardShownMs_ = 0;
      dirty_ = true;
    }
  }
  if (overlay_ == ScreenId::PowerOff && now - overlayAt_ >= ms(kPowerOffCardMs)) dirty_ = true;
  // Diagnostics > Buttons: any key held 2 s, or 10 s without input, releases the capture -> page 4 (MAG RAW)
  if (ui_.depth && ui_.modal[ui_.depth - 1] == ScreenId::Diagnostics && ui_.diagPage == 2) {
    bool exit = lastCaptureEvent_.v && now - lastCaptureEvent_ >= ms(kButtonsExitIdleMs);
    for (uint8_t k = 0; k < kKeyCount; ++k)
      if ((capDown_ >> k) & 1u && now - downSince_[k] >= ms(kButtonsExitHoldMs)) exit = true;
    if (!lastCaptureEvent_.v) lastCaptureEvent_ = now;
    if (exit) { ui_.diagPage = 3; capDown_ = 0; lastCaptureEvent_ = MonoUs{0}; dirty_ = true; }
  }
}

uint32_t UiController::vmSum(const ViewModel& vm) const { return crc32(&vm, sizeof vm); }

bool UiController::wantsFrame(const ViewModel& vm, MonoUs now) const {
  if (!d_.ok()) return false;
  if (dirty_ || !rendered_ || now.v >= nextFrameAt_.v) return true;
  return now - lastRender_ >= 33 * kUsPerMs && vmSum(vm) != lastSum_;
}

void UiController::render(const ViewModel& vm, MonoUs now) {
  const uint16_t rej0 = c_.rejected();
  c_.clear();
  const ScreenId top = topScreen(vm);
  const Screen& s = screenFor(top);
  Bindings b{};
  s.bindings(vm, ui_, b);
  const bool carousel = top == pageScreen(ui_.page) && ui_.depth == 0;
  if (!b.railHidden && !b.captureAll) c_.clipContent();   // captureAll screens draw their own rail boxes
  s.render(c_, vm, ui_, now);
  c_.clipNone();
  float slideT = 1.0f;
  if (ui_.sliding && carousel) {
    const uint8_t d = slideOffset(now - ui_.slideAt, ui_.animations ? ms(kSlideMs) : 0);
    composeSlide(slideOld_, c_.buffer(), d, ui_.slideDir, (uint8_t)kContentX1);
    slideT = (float)(now - ui_.slideAt) / (float)ms(kSlideMs);
    if (d >= 64) ui_.sliding = false;
  } else {
    ui_.sliding = false;
  }
  widgets::RailState r{};
  r.dots = carousel;
  r.page = ui_.page;
  r.pageCount = kPages;
  r.fromPage = slideFrom_;
  r.slideT = slideT > 1.0f ? 1.0f : slideT;
  r.pressedMask = ui_.pressedMask;
  r.hold = ui_.hold;
  widgets::rail(c_, b, r);
  if (ui_.toastCount && !toastPaused(displayOn_)) {
    char t[48];
    toastText(t, sizeof t);
    const uint16_t ms_ = ui_.toastShownMs;
    int16_t dy = 0;
    if (ui_.animations) {
      if (ms_ < kToastSlideMs) dy = (int16_t)(15 - 15 * ms_ / kToastSlideMs);
      else if (ms_ > kToastSlideMs + kToastMs) dy = (int16_t)(15 * (ms_ - kToastSlideMs - kToastMs) / kToastSlideMs);
    }
    widgets::toast(c_, t, dy);
  }
  rejected_ = (uint16_t)(c_.rejected() - rej0);
  lastRender_ = now;
  rendered_ = true;
  dirty_ = false;
  lastSum_ = vmSum(vm);
  uint16_t period = s.framePeriodMs(vm, ui_);
  if (ui_.sliding || (ui_.toastCount && ui_.animations)) period = 33;
  if (period == 0) period = 1000;
  nextFrameAt_ = now + (int64_t)period * kUsPerMs;
}

bool UiController::flush(uint32_t budgetUs, MonoUs now) {
  (void)budgetUs; (void)now;
  if (!d_.ok()) return true;
  PageSpan spans[kFlushMaxSpans];
  const uint8_t n = diff_.collect(c_.buffer(), spans, kFlushMaxSpans);
  for (uint8_t i = 0; i < n; ++i) d_.sendPageSpan(spans[i].page, spans[i].tx0, spans[i].tw);
  return diff_.clean(c_.buffer());
}

void UiController::forceFullRefresh() { diff_.invalidate(); dirty_ = true; }

uint8_t UiController::sensorDemand(const ViewModel& vm) const { return screenFor(topScreen(vm)).sensorDemand(vm, ui_); }

bool UiController::keepAwake() const {
  if (ui_.card == ErrCode::I2cStuck) return true;                          // FATAL card
  for (uint8_t i = 0; i < ui_.depth; ++i)
    if (ui_.modal[i] == ScreenId::Calibration) return true;               // capped at 120 s by the app
  return ui_.depth && ui_.modal[ui_.depth - 1] == ScreenId::Diagnostics && ui_.diagPage == 2;   // Buttons page
}

bool UiController::animating(MonoUs) const { return ui_.sliding || (ui_.toastCount && ui_.animations && displayOn_); }
MonoUs UiController::nextFrameAt() const { return nextFrameAt_; }
const UiState& UiController::state() const { return ui_; }
UiState& UiController::stateForTest() { dirty_ = true; return ui_; }
uint32_t UiController::toastsDropped() const { return toastsDropped_; }
uint16_t UiController::rejectedLastFrame() const { return rejected_; }

const char* UiController::toastText(char* buf, size_t n) const {
  if (!n) return buf;
  buf[0] = 0;
  if (!ui_.toastCount) return buf;
  const Toast& t = ui_.toastQ[ui_.toastHead];
  snprintf(buf, n, str(t.str), t.arg);
  return buf;
}

}  // namespace hg
