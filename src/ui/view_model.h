#pragma once
#include "../core/civil_time.h"
#include "../core/clock_policy.h"
#include "../core/countdown.h"
#include "../core/heater_policy.h"
#include "../core/input.h"
#include "../core/mag_calibrator.h"
#include "../core/settings.h"
#include "../core/stopwatch.h"
#include "bindings.h"
#include "view_types.h"
namespace hg {
struct ClockView   { bool valid; int64_t utcS; Civil local; char zone[6]; int16_t offsetMin; ClockQuality q; ClockSource src; int32_t syncAgeS; };
struct ThermoView  { Health h; ShtChip chip; uint8_t addr, idByte; uint32_t serial; bool valid, stale; float tC, rh, dewC, ahGm3;
                     uint8_t heaterUi /*0 off,1 heating,2 cooling*/; HeaterDecision lastSkip; float lastHotC, lastPreC; ErrCode err; };
struct MagView     { Health h; MagChip chip; uint8_t addr, idByte; bool idWarning, valid, calibrated, trueRef, saturated, fieldSuspect;
                     float headingDeg; int16_t shownDeg; float cardDeg; int16_t raw[3], mn[3], mx[3]; float fieldUt;
                     uint16_t rateHz; uint32_t drdyPerS; DrdyKind drdy; ErrCode err; };
struct StopwatchView { Stopwatch::State st; int64_t elapsedUs, curLapUs; uint16_t lapNo; uint8_t laps; int64_t lastLapUs, lastDeltaUs; };
struct TimerView   { Countdown::State st; uint8_t preset; int64_t remainingUs, totalUs, overtimeUs; bool endsKnown; Civil endsLocal;
                     bool alertActive; uint8_t alertPhase; bool missed; };
struct TargetView  { bool set; int16_t bearing; float steerDeg; };
struct CalView     { MagCalibrator::Progress p; bool haveResult; MagCalibrator::Result r; };
struct SystemView  { const char* fw; const char* build; ResetReason resetReason; WakeCause wakeCause; uint8_t wakeKey;
                     uint32_t wakeSampleUs; int64_t uptimeS; uint32_t heapFree;
                     bool usbHost, cdcConnected; uint32_t loopMaxUs, flushUs; I2cStats bus0, bus1; uint8_t scan0[8], n0, scan1[4], n1;
                     uint16_t keyPresses[kKeyCount]; uint8_t keyDownMask, keyStuckMask; uint32_t serialDropped;
                     const char* coreVer; ErrCode busErr, nvsErr;   // impl. note: CHECK HARDWARE rows, Diagnostics
                     uint16_t heatPreC10, heatHotC10; bool heatValid; };
struct BgChips     { bool warn, sw, tmr, bell, syncTag; int32_t tmrRemainS; };   // syncTag: clock Approx (Home only, §3.4)
struct ViewModel   { ClockView clock; ThermoView thermo; MagView mag; StopwatchView sw; TimerView tm; TargetView target;
                     BatteryState bat; CalView cal; SystemView sys; BgChips bg; const Settings* settings; FormatCtx fmt; };
struct Toast       { uint16_t str; char arg[16]; };
struct UiState     { uint8_t page; bool details; ScreenId modal[2]; uint8_t depth; SettingId listCursor; uint8_t listTop;
                     int16_t pending; uint8_t timeField; Civil timeEdit; uint8_t diagPage;
                     Toast toastQ[4]; uint8_t toastHead, toastCount; uint16_t toastShownMs;  // queue §2.3.7; visible time so far
                                                                                              // (does not advance while paused)
                     bool sliding; int8_t slideDir; MonoUs slideAt; HoldState hold; uint8_t pressedMask;
                     ErrCode card; uint8_t cardArg; bool animations; };
}  // namespace hg
