#pragma once
// AppCore: owns the models, services and the UI; boot() and step() (SPEC §4.5, §4.6). Pure composition over the
// ports, so the whole firmware logic runs on the PC with fakes (test_app_scenarios).
#include "../core/countdown.h"
#include "../core/health.h"
#include "../core/input.h"
#include "../core/settings.h"
#include "../core/stopwatch.h"
#include "../ui/canvas.h"
#include "../ui/ui_controller.h"
#include "../ui/view_model.h"
#include "compass_service.h"
#include "logger.h"
#include "persist.h"
#include "ports.h"
#include "power_manager.h"
#include "thermo_service.h"
#include "time_service.h"

namespace hg {
struct CmdLine;
struct BootInputs { const char* buildDate; const char* buildTime; const char* coreVersion = nullptr; };   // __DATE__, __TIME__ from the .ino
struct Directive { enum class Sleep : uint8_t { None, Light, Deep } sleep; uint32_t lightUs; uint8_t excludeKeys /*stuck keys*/;
                   DeepSleepPlan deep; uint16_t idleMs; };
class AppCore {
 public:
  explicit AppCore(const Ports& p);
  void boot(const BootInputs& in);           // §4.6 steps 5-12
  Directive step();                          // one loop iteration; never blocks > ~10 ms
  void onDeepSleepFailed(SleepResult r);     // re-init buses/display, toast "Sleep failed", log E-PWR-SLEEP, stay awake
  void noteLightSleep(SleepResult r);        // counters for Diagnostics; Rejected/TooShort = immediate wake
  const ViewModel& viewModel() const;        // host tests

  // host tests and the console
  UiController& ui() { return ui_; }
  const Settings& settings() const { return settings_; }
  Logger& logger() { return log_; }
  const Stopwatch& stopwatch() const { return sw_; }
  const Countdown& countdown() const { return tm_; }
  const HealthRegistry& health() const { return health_; }
  bool alertActive() const { return alertActive_; }
  DisplayState displayState() const { return power_.display(); }
  void execute(const CmdLine& c);            // console.cpp

 private:
  enum class Shut : uint8_t { Idle, Card, Quiesce, Keys, Ready };
  void act(const UiCommand& c, MonoUs now);
  void handleKeys(MonoUs now);
  void tickModels(MonoUs now);
  void tickSensors(MonoUs now);
  void tickSerial(MonoUs now);
  void tickAlert(MonoUs now);
  void raiseAlert(MonoUs now);
  void dismissAlert(MonoUs now);
  bool startShutdown(MonoUs now, bool manual);
  bool tickShutdown(MonoUs now, Directive& d);
  void snapshot(MonoUs now);
  void buildViewModel(MonoUs now);
  void applySettingSideEffects(SettingId id);
  bool saveSetting(SettingId id, int16_t v, MonoUs now);
  void nvsFailed(const char* key, MonoUs now);
  Settings liveSettings() const;             // the saved settings with the open picker's pending value (live preview)
  const TzDef& tz() const;
  Brightness brightness() const;
  uint32_t screenTimeoutMs() const;
  uint32_t autoOffMs() const;
  ThermoCtx thermoCtx(bool fast) const;
  CompassCtx compassCtx(bool demand) const;
  void toastTimerLeft(MonoUs now);
  void scanBuses();
  friend struct ConsoleAccess;

  Ports p_;
  HealthRegistry health_;
  Logger log_;
  Persist persist_;
  TimeService time_;
  ThermoService thermo_;
  CompassService compass_;
  PowerManager power_;
  Canvas canvas_;
  UiController ui_;
  InputLogic input_;
  Stopwatch sw_;
  Countdown tm_;
  Settings settings_{};
  ViewModel vm_{};
  Settings vmSettings_{};
  BootInputs bootIn_{};
  WakeInfo wake_{};
  RestoreVerdict verdict_ = RestoreVerdict::Fresh;
  MonoUs bootAt_{0}, lastBattery_{0}, lastTlm_{0}, magStreamUntil_{0}, alertAt_{0}, lastFlip_{0}, shutAt_{0};
  BatteryState bat_{false, 0, 0};
  bool alertActive_ = false, invertOn_ = false, missedBell_ = false, deferredOff_ = false, shutManual_ = false;
  bool splash_ = false, checkHwShown_ = false, shtInfoChecked_ = false, hostWasConnected_ = false, nvsCardShown_ = false;
  bool lastDisplayOn_ = true, crashCleared_ = false, rtcWasValid_ = false, displayWasOff_ = false;
  Shut shut_ = Shut::Idle;
  uint8_t shutStuck_ = 0;
  uint32_t loopMaxUs_ = 0, flushUs_ = 0, tlmS_ = 0;
  uint16_t keyPresses_[kKeyCount] = {};
  uint32_t modelRev_ = 0, snapRev_ = 0;
  char rx_[100] = {};
  size_t rxLen_ = 0;
  bool rxOverflow_ = false;
  uint8_t scan0_[8] = {}, n0_ = 0, scan1_[4] = {}, n1_ = 0;
  ShtChip prevSht_ = ShtChip::None;
  uint32_t magStreamN_ = 0;
  bool drdyOn_ = false, displayOk_ = true;
  uint32_t drdyLast_ = 0, drdyRate_ = 0, busRec_ = 0, busFail_ = 0;
  MonoUs drdyAt_{0};
  Health prevHealth_[4] = {Health::Unknown, Health::Unknown, Health::Unknown, Health::Unknown};   // Sht, Mag, Bus0, Nvs
};
}  // namespace hg
