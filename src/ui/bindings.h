#pragma once
#include "../core/input.h"
#include "icons.h"
namespace hg {
enum class ActionId : uint8_t {
  None,
  // handled inside UiController
  PagePrev, PageNext, DetailsToggle, OpenList, ListUp, ListDown, ListBack, ListActivate, PickUp, PickDown,
  PickCancel, TimeInc, TimeDec, TimeFieldPrev, TimeFieldNext, DiagPrev, DiagNext, DiagBack, CardDismiss,
  AlertDismiss, ShowHint,
  // returned to AppCore as UiCommand
  DisplayOff, PowerOff, SwStart, SwPause, SwResume, SwLap, SwReset, TmNextPreset, TmStart, TmPause, TmResume,
  TmAddMinute, TmCancel, TmNew, TmRestart, CmpMark, CmpClear, CmpCalibrate, CmpRetry, CalCancel, CalSave, CalRetry,
  CalDone, SettingSave, SetTimeCommit, ResetSettings, Reboot, HeaterTest, Rescan, MagMinMaxReset, Count };
enum class LabelMode : uint8_t { None, Static, PillOnHold };
struct Binding { ActionId shortAct = ActionId::None, longAct = ActionId::None; uint16_t holdMs = 0;
                 bool repeat = false, accel = false; IconId icon = IconId::None, holdIcon = IconId::None;
                 uint16_t labelStr = 0; LabelMode labelMode = LabelMode::None; uint16_t hintStr = 0; };
struct Bindings { Binding key[kKeyCount]; bool chevrons = false; bool captureAll = false; bool railHidden = false; };
KeyConfig keyConfigFor(const Binding& b);   // trigger rule §2.2
struct UiCommand { ActionId action; int32_t arg; MonoUs tDown; };
}  // namespace hg
