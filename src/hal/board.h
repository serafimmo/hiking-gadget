#pragma once
// Composition root of the device build: creates the hal ports and the AppCore, runs the loop (§4.5, §4.6).
namespace hg {
void boardSetup(const char* buildDate, const char* buildTime);
void boardLoop();
}  // namespace hg
