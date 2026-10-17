#include "cli.h"

#include <string.h>

#include "tz.h"

namespace hg {
namespace {
constexpr size_t kMaxLine = 96;

char up(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }
char low(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

bool wordIs(const char* w, size_t n, const char* name) {
  size_t i = 0;
  for (; i < n && name[i]; ++i)
    if (up(w[i]) != name[i]) return false;
  return i == n && !name[i];
}

bool parseInt(const char* s, int32_t lo, int32_t hi, int32_t& out) {
  if (!s || !*s) return false;
  bool neg = false;
  if (*s == '-' || *s == '+') neg = *s++ == '-';
  if (!*s) return false;
  int64_t v = 0;
  for (; *s; ++s) {
    if (*s < '0' || *s > '9') return false;
    v = v * 10 + (*s - '0');
    if (v > 1000000000) return false;
  }
  if (neg) v = -v;
  if (v < lo || v > hi) return false;
  out = (int32_t)v;
  return true;
}

CmdLine error(const char* why) {
  CmdLine c{};
  c.cmd = Cmd::Error;
  c.err = why;
  return c;
}

// Copies at most cap-1 characters; false if the token does not fit.
bool copyTok(char* dst, size_t cap, const char* src, bool lower) {
  size_t i = 0;
  for (; src[i]; ++i) {
    if (i + 1 >= cap) return false;
    dst[i] = lower ? low(src[i]) : src[i];
  }
  dst[i] = 0;
  return true;
}
}  // namespace

CmdLine parseLine(const char* line) {
  CmdLine c{};
  c.cmd = Cmd::None;
  if (!line) return c;
  // Trim, check the length, split "WORD rest".
  while (space(*line)) ++line;
  size_t n = strlen(line);
  while (n && space(line[n - 1])) --n;
  if (n == 0) return c;
  if (n > kMaxLine) return error("LINE too long (max 96 characters)");
  char buf[kMaxLine + 1];
  memcpy(buf, line, n);
  buf[n] = 0;
  size_t wn = 0;
  while (wn < n && !space(buf[wn])) ++wn;
  const char* rest = buf + wn;
  while (space(*rest)) ++rest;
  const bool noArg = *rest == 0;
  // the first space-separated argument, and whatever follows it
  char arg1[24] = {};
  const char* rest2 = rest;
  {
    size_t i = 0;
    while (*rest2 && !space(*rest2)) {
      if (i + 1 < sizeof arg1) arg1[i++] = *rest2;
      ++rest2;
    }
    while (space(*rest2)) ++rest2;
  }

  if (wordIs(buf, wn, "PING") && noArg) { c.cmd = Cmd::Ping; return c; }
  if (wordIs(buf, wn, "HELP") && noArg) { c.cmd = Cmd::Help; return c; }
  if (wordIs(buf, wn, "INFO") && noArg) { c.cmd = Cmd::Info; return c; }
  if ((wordIs(buf, wn, "TIME?") || wordIs(buf, wn, "T")) && noArg) { c.cmd = Cmd::TimeQuery; return c; }
  if (wordIs(buf, wn, "TIME")) {
    if (noArg || (rest[0] == '?' && rest[1] == 0)) { c.cmd = Cmd::TimeQuery; return c; }
    if (parseUnixMs(rest, c.unixMs)) { c.cmd = Cmd::TimeUnix; return c; }
    if (parseLocalDateTime(rest, c.local)) { c.cmd = Cmd::TimeLocal; return c; }
    return error("TIME FORMAT expected 'TIME <unix>[.ms]' or 'TIME YYYY-MM-DD HH:MM[:SS]'");
  }
  if ((wordIs(buf, wn, "TZ?") && noArg) || (wordIs(buf, wn, "TZ") && (noArg || (rest[0] == '?' && !rest[1])))) {
    c.cmd = Cmd::TzQuery;
    return c;
  }
  if (wordIs(buf, wn, "TZ")) {
    if (*rest2 || findTz(arg1) < 0) return error("TZ UNKNOWN zone id, send 'TZ?' for the list");
    for (size_t i = 0; arg1[i] && i < sizeof c.key - 1; ++i) c.key[i] = up(arg1[i]);
    c.cmd = Cmd::TzSet;
    return c;
  }
  if (wordIs(buf, wn, "SCAN") && noArg) { c.cmd = Cmd::Scan; return c; }
  if (wordIs(buf, wn, "SHT") && noArg) { c.cmd = Cmd::Sht; return c; }
  if (wordIs(buf, wn, "HEAT") && noArg) { c.cmd = Cmd::Heat; return c; }
  if (wordIs(buf, wn, "KEYS") && noArg) { c.cmd = Cmd::Keys; return c; }
  if (wordIs(buf, wn, "MAG")) {
    if (*rest2 || !parseInt(arg1, 0, 60, c.num)) return error("MAG SECONDS expected 'MAG <0..60>'");
    c.cmd = Cmd::Mag;
    return c;
  }
  if (wordIs(buf, wn, "GET")) {
    if (noArg || *rest2 || !copyTok(c.key, sizeof c.key, arg1, true)) return error("GET KEY expected 'GET <key>'");
    c.cmd = Cmd::Get;
    return c;
  }
  if (wordIs(buf, wn, "SET")) {
    if (noArg || !*rest2 || !copyTok(c.key, sizeof c.key, arg1, true) || !copyTok(c.val, sizeof c.val, rest2, false))
      return error("SET FORMAT expected 'SET <key> <value>'");
    c.cmd = Cmd::Set;
    return c;
  }
  if (wordIs(buf, wn, "LOG")) {
    if (!noArg && !*rest2 && wordIs(arg1, strlen(arg1), "DUMP")) { c.cmd = Cmd::LogDump; return c; }
    const char l = up(arg1[0]);
    if (!*rest2 && arg1[0] && !arg1[1] && (l == 'E' || l == 'W' || l == 'I' || l == 'D')) {
      c.cmd = Cmd::Log;
      c.key[0] = l;
      return c;
    }
    return error("LOG LEVEL expected 'LOG E|W|I|D' or 'LOG DUMP'");
  }
  if (wordIs(buf, wn, "TLM")) {
    if (*rest2 || !parseInt(arg1, 0, 3600, c.num)) return error("TLM SECONDS expected 'TLM <0..3600>' (0 = off)");
    c.cmd = Cmd::Tlm;
    return c;
  }
  if (wordIs(buf, wn, "REBOOT") && noArg) { c.cmd = Cmd::Reboot; return c; }
  if (wordIs(buf, wn, "FACTORY")) {
    if (!*rest2 && wordIs(arg1, strlen(arg1), "YES")) { c.cmd = Cmd::Factory; return c; }
    return error("FACTORY CONFIRM send 'FACTORY YES' to erase all settings");
  }
  return error("UNKNOWN command, send HELP for the list");
}

}  // namespace hg
