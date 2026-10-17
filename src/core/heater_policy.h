#pragma once
#include "types.h"
namespace hg {
enum class HeaterMode  : uint8_t { Off, Auto, Every5, Every10, Every30, Every60 };
enum class HeaterPower : uint8_t { Low20mW, Mid110mW, High200mW };
struct HeaterIn { HeaterMode mode; bool supported; bool busy; MonoUs now; bool havePulse; MonoUs lastPulse;
                  bool tValid; float tC; uint8_t highRhStreak;
                  bool shutdownPending; };   // deep sleep requested or the sequencer is running -> never start a pulse
enum class HeaterDecision : uint8_t { No, Pulse, SkipHot, SkipUnsupported, SkipSpacing, SkipShutdown };
HeaterDecision heaterDue(const HeaterIn& in);   // rules §6.2
uint8_t heaterCommand(HeaterPower p);           // 0x1E / 0x2F / 0x39 (1 s pulses)
}  // namespace hg
