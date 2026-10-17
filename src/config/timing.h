#pragma once
// Every duration of the firmware (SPEC Appendix A). Milliseconds unless the name ends in Us.
#include <stdint.h>
namespace hg {
// Keys and gestures (§2.2)
constexpr uint32_t kDebounceMs = 15, kClickMaxMs = 400, kHoldVisualMs = 150, kHoldMs = 800, kHoldCriticalMs = 1500;
constexpr uint32_t kRepeatDelayMs = 400, kRepeatMs = 125, kRepeatFastMs = 50, kRepeatFastAfterMs = 1500, kAccelAfterMs = 2000;
constexpr uint8_t  kAccelStep = 5;
constexpr uint32_t kPressedMinMs = 80, kButtonsExitHoldMs = 2000, kButtonsExitIdleMs = 10000;
// Display, motion, toasts (§3.6, §2.3)
constexpr uint32_t kDimLeadMs = 5000, kDimLeadShortMs = 3000, kFadeInMs = 120, kFadeOutMs = 250;
constexpr uint32_t kToastMs = 1200, kToastSlideMs = 100;
constexpr uint8_t  kToastQueue = 4;
constexpr uint32_t kSlideMs = 180;
constexpr uint32_t kAlertFlashMs = 30000, kAlertFlashPeriodMs = 500, kAlertHoldMs = 300000;
constexpr uint32_t kCardAutoDismissMs = 8000, kSplashMinMs = 600, kSplashMaxMs = 1500, kPowerOffCardMs = 1000;
// SHT (§6.2)
constexpr uint32_t kShtHomePeriodMs = 2000, kShtOnPeriodMs = 10000, kShtAutoDarkPeriodMs = 60000, kShtStaleMs = 60000;
constexpr uint32_t kShtMeasWaitMs = 10, kSht3xMeasWaitMs = 16, kShtHeaterWaitMs = 1110, kShtReadDeadlineMs = 30,
                   kShtHeaterDeadlineMs = 90;
constexpr uint32_t kShtProbeSerialRetryMs = 10, kShtHeaterBusyMaxMs = 1200, kHeaterMinSpacingMs = 60000,
                   kHeaterAutoMinGapMs = 600000;
constexpr uint32_t kHeaterCoolMinMs = 20000, kHeaterCoolMaxMs = 90000;
constexpr float    kHeaterMaxAmbientC = 60.0f, kHeaterAutoRh = 90.0f;
// Magnetometer and calibration (§6.3, §6.4)
constexpr uint32_t kMagPollMs = 20, kMagStandbyGraceMs = 2000, kMagStuckMs = 500;
constexpr uint8_t  kMagStuckSamples = 25;
constexpr uint32_t kMagNoStatusMs = 500, kQmcResetWaitMs = 10, kQmcPResetWaitMs = 5;
constexpr uint32_t kCalMinMs = 8000, kCalTimeoutMs = 120000, kCalKeepAwakeMaxMs = 120000;
// Health (§9)
constexpr uint32_t kReprobeBackoffMs[4] = {5000, 10000, 30000, 60000};
constexpr uint8_t  kI2cMaxRecoveriesPerMin = 3;
constexpr uint32_t kI2cStuckRecheckMs = 30000;   // a stuck bus stays down until a line check passes (§6.1)
// Power (§7)
constexpr uint32_t kLightSleepMinMs = 50, kLightSleepMaxSliceMs = 4000, kTimerWakeLeadMs = 1500, kAutoOffTimerVetoMs = 60000;
constexpr uint32_t kUsbPlugOnMs = 1000, kUsbPlugOffMs = 3000;
constexpr uint8_t  kUsbProbeEverySlices = 8;
constexpr uint32_t kUsbProbeAwakeMs = 300, kKeyReleaseWaitMs = 3000;
constexpr uint8_t  kPhantomWakeLimit = 5;
constexpr uint32_t kPhantomWindowMs = 120000, kPhantomMaxSleepMs = 3000, kKeyStuckMs = 10000, kKeyStuckClearMs = 1000;
// Loop, persistence, frames (§4.5)
constexpr uint32_t kClockCheckpointMs = 3600000, kFullRefreshMs = 60000, kFlushBudgetUs = 8000;
constexpr uint8_t  kFlushMaxSpans = 2;
constexpr uint32_t kMaxIdleMs = 5;
constexpr uint32_t kCrashRestoreUptimeMs = 60000;
constexpr uint8_t  kCrashRestoreMax = 3;
}  // namespace hg
