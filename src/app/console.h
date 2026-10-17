#pragma once
// Serial console (SPEC §5.6, §9.2, §9.3): command execution lives in AppCore::execute (console.cpp); these start
// the long outputs that the Logger paces line by line.
namespace hg {
class AppCore;
void logStartBootReport(AppCore& app);   // boot report (always; replayed by INFO and on host connect)
void logStartReplay(AppCore& app);       // boot report + the log ring
void logTelemetry(AppCore& app);         // one TLM line
}  // namespace hg
