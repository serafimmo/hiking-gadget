#pragma once
#include "civil_time.h"
namespace hg {
enum class Cmd : uint8_t { None, Ping, Help, Info, TimeUnix, TimeLocal, TimeQuery, TzSet, TzQuery, Scan, Sht, Heat,
                           Mag, Keys, Get, Set, Log, LogDump, Tlm, Reboot, Factory, Error };
struct CmdLine { Cmd cmd; int64_t unixMs; Civil local; char key[8]; char val[16]; int32_t num; const char* err; };
CmdLine parseLine(const char* line);   // case-insensitive, trims, <= 96 chars, never fails silently (Error + err text)
}  // namespace hg
