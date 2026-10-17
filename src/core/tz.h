#pragma once
#include "civil_time.h"
namespace hg {
enum class DstRule : uint8_t { None, EU, US };
struct TzDef { const char* id; const char* stdAbbr; const char* dstAbbr; const char* title /*caps, picker value*/;
               const char* note /*sub-label after "UTC+h"*/; int16_t stdOffsetMin; DstRule dst; };   // §5.5; ASCII, checked by test_strings
constexpr uint8_t kTzCount = 18, kTzDefault = 7;        // index 7 = "MSK"
extern const TzDef kTzTable[kTzCount];
int     findTz(const char* id);                          // -1 if unknown
int16_t utcOffsetMin(const TzDef& tz, int64_t utcS, bool* isDst = nullptr);
Civil   utcToLocal(int64_t utcS, const TzDef& tz);
int64_t localToUtc(const Civil& local, const TzDef& tz); // spring-forward gap -> +60 min; autumn overlap -> earlier instant
const char* tzAbbr(const TzDef& tz, int64_t utcS);
}  // namespace hg
