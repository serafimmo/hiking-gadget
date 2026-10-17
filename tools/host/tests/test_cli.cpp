// core/cli: every command form, garbage -> Error with a reason, length limit (§9.3).
#include <cstring>

#include "core/cli.h"
#include "tiny_test.h"

using namespace hg;

TEST(cli_simple_commands) {
  CHECK_EQ(parseLine("PING").cmd, Cmd::Ping);
  CHECK_EQ(parseLine("  ping \r\n").cmd, Cmd::Ping);
  CHECK_EQ(parseLine("help").cmd, Cmd::Help);
  CHECK_EQ(parseLine("Info").cmd, Cmd::Info);
  CHECK_EQ(parseLine("SCAN").cmd, Cmd::Scan);
  CHECK_EQ(parseLine("sht").cmd, Cmd::Sht);
  CHECK_EQ(parseLine("HEAT").cmd, Cmd::Heat);
  CHECK_EQ(parseLine("KEYS").cmd, Cmd::Keys);
  CHECK_EQ(parseLine("REBOOT").cmd, Cmd::Reboot);
  CHECK_EQ(parseLine("factory yes").cmd, Cmd::Factory);
  CHECK_EQ(parseLine("FACTORY").cmd, Cmd::Error);
  CHECK_EQ(parseLine("").cmd, Cmd::None);
  CHECK_EQ(parseLine("   ").cmd, Cmd::None);
}

TEST(cli_time_forms) {
  CmdLine c = parseLine("TIME 1791736927.250");
  CHECK_EQ(c.cmd, Cmd::TimeUnix); CHECK_EQ(c.unixMs, (int64_t)1791736927250);
  c = parseLine("time 1791736927");
  CHECK_EQ(c.cmd, Cmd::TimeUnix); CHECK_EQ(c.unixMs, (int64_t)1791736927000);
  c = parseLine("TIME 2026-10-11 19:42");
  CHECK_EQ(c.cmd, Cmd::TimeLocal); CHECK_EQ(c.local.hour, 19); CHECK_EQ(c.local.second, 0);
  c = parseLine("TIME 2026-10-11T19:42:07");
  CHECK_EQ(c.cmd, Cmd::TimeLocal); CHECK_EQ(c.local.second, 7);
  CHECK_EQ(parseLine("TIME?").cmd, Cmd::TimeQuery);
  CHECK_EQ(parseLine("T").cmd, Cmd::TimeQuery);
  CHECK_EQ(parseLine("time ?").cmd, Cmd::TimeQuery);
  c = parseLine("TIME yesterday");
  CHECK_EQ(c.cmd, Cmd::Error);
  CHECK(c.err && strncmp(c.err, "TIME FORMAT", 11) == 0);
}

TEST(cli_tz_get_set_log_tlm_mag) {
  CmdLine c = parseLine("tz msk");
  CHECK_EQ(c.cmd, Cmd::TzSet); CHECK_STR(c.key, "MSK");
  CHECK_EQ(parseLine("TZ?").cmd, Cmd::TzQuery);
  CHECK_EQ(parseLine("TZ").cmd, Cmd::TzQuery);
  CHECK_EQ(parseLine("TZ XYZ").cmd, Cmd::Error);
  c = parseLine("GET Scr");
  CHECK_EQ(c.cmd, Cmd::Get); CHECK_STR(c.key, "scr");
  c = parseLine("SET toff -12");
  CHECK_EQ(c.cmd, Cmd::Set); CHECK_STR(c.key, "toff"); CHECK_STR(c.val, "-12");
  CHECK_EQ(parseLine("SET toff").cmd, Cmd::Error);
  CHECK_EQ(parseLine("GET").cmd, Cmd::Error);
  CHECK_EQ(parseLine("GET waytoolongkey").cmd, Cmd::Error);
  c = parseLine("LOG d");
  CHECK_EQ(c.cmd, Cmd::Log); CHECK_EQ(c.key[0], 'D');
  CHECK_EQ(parseLine("LOG DUMP").cmd, Cmd::LogDump);
  CHECK_EQ(parseLine("LOG X").cmd, Cmd::Error);
  c = parseLine("TLM 5");
  CHECK_EQ(c.cmd, Cmd::Tlm); CHECK_EQ(c.num, 5);
  CHECK_EQ(parseLine("TLM -1").cmd, Cmd::Error);
  c = parseLine("MAG 60");
  CHECK_EQ(c.cmd, Cmd::Mag); CHECK_EQ(c.num, 60);
  CHECK_EQ(parseLine("MAG 61").cmd, Cmd::Error);
  CHECK_EQ(parseLine("MAG").cmd, Cmd::Error);
}

TEST(cli_garbage_and_length_limit) {
  CmdLine c = parseLine("FOO bar");
  CHECK_EQ(c.cmd, Cmd::Error); CHECK(c.err && strlen(c.err) > 0);
  char longLine[120];
  memset(longLine, 'A', sizeof longLine);
  longLine[97] = 0;
  c = parseLine(longLine);
  CHECK_EQ(c.cmd, Cmd::Error); CHECK(strncmp(c.err, "LINE", 4) == 0);
  memcpy(longLine, "PING", 4);
  for (int i = 4; i < 96; ++i) longLine[i] = ' ';
  longLine[96] = 0;                                         // 96 characters, trailing spaces trimmed
  CHECK_EQ(parseLine(longLine).cmd, Cmd::Ping);
  CHECK_EQ(parseLine(nullptr).cmd, Cmd::None);
}
