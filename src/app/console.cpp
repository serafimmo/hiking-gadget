#include "console.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../config/board_pins.h"
#include "../config/version.h"
#include "../core/cli.h"
#include "../ui/fmt.h"
#include "../ui/strings.h"
#include "app_core.h"

namespace hg {

struct ConsoleAccess {
  enum class Long : uint8_t { Boot, Replay, Help, Info, Zones, Dump };
  static Long mode;
  static AppCore* app;
  static uint16_t ringStart;

  static const char* qName(ClockQuality q) {
    return q == ClockQuality::Synced ? "SYNCED" : q == ClockQuality::Manual ? "MANUAL" : q == ClockQuality::Approx ? "APPROX" : "NONE";
  }
  static const char* srcName(ClockSource s) {
    static const char* const k[] = {"none", "build", "restored", "manual", "serial"};
    return k[(int)s < 5 ? (int)s : 0];
  }
  static const char* resetName(ResetReason r) {
    static const char* const k[16] = {"UNKNOWN", "POWERON", "EXT", "SW", "PANIC", "INT_WDT", "TASK_WDT", "WDT",
                                      "DEEPSLEEP", "BROWNOUT", "SDIO", "USB", "JTAG", "EFUSE", "PWR_GLITCH", "CPU_LOCKUP"};
    return (uint8_t)r < 16 ? k[(uint8_t)r] : "?";
  }
  static void timeLine(AppCore& a, char* b, size_t n, bool query) {
    const int64_t ms = a.time_.utcMs();
    const Civil u = civilFromEpoch(ms / 1000), l = utcToLocal(ms / 1000, a.tz());
    char age[16];
    const int64_t s = a.time_.syncAgeS();
    snprintf(age, sizeof age, "%dd%02dh", (int)(s / 86400), (int)(s / 3600 % 24));
    snprintf(b, n, "%sutc=%04d-%02d-%02dT%02d:%02d:%02d.%03dZ local=%02d:%02d:%02d %s q=%s%s%s age=%s", query ? "TIME " : "",
             u.year, u.month, u.day, u.hour, u.minute, u.second, (int)(ms % 1000), l.hour, l.minute, l.second,
             tzAbbr(a.tz(), ms / 1000), qName(a.time_.meta().q), query ? " src=" : "", query ? srcName(a.time_.meta().src) : "", age);
  }

  static bool bootLine(AppCore& a, uint16_t i, char* b, size_t n) {
    const ViewModel& v = a.vm_;
    switch (i) {
      case 0: snprintf(b, n, "=== Hiking Gadget %s  build \"%s %s\" id %08lX  core %s ===", kFwVersion,
                       a.bootIn_.buildDate ? a.bootIn_.buildDate : "?", a.bootIn_.buildTime ? a.bootIn_.buildTime : "?",
                       (unsigned long)a.time_.buildId(), a.bootIn_.coreVersion ? a.bootIn_.coreVersion : "host"); return true;
      case 1: snprintf(b, n, "reset %s  wake %s  sample %lums  restore %s", resetName(a.wake_.resetReason),
                       a.wake_.cause == WakeCause::Ext1 ? "EXT1" : a.wake_.cause == WakeCause::Timer ? "TIMER" : "-",
                       (unsigned long)(a.wake_.sampleUs / 1000),
                       a.verdict_ == RestoreVerdict::Restore ? "RESTORE" : a.verdict_ == RestoreVerdict::RestoreCrash ? "RESTORE_CRASH"
                       : a.verdict_ == RestoreVerdict::Lost ? "LOST" : a.verdict_ == RestoreVerdict::CrashLoop ? "CRASH_LOOP" : "FRESH"); return true;
      case 2: {
        const uint8_t lv = a.p_.keys ? a.p_.keys->rawPressedMask() : 0;
        snprintf(b, n, "keys K1=GPIO%u(^) K2=GPIO%u(v) K3=GPIO%u(#) K4=GPIO%u(*)  levels(1=up) %d%d%d%d  stuck %02X",
                 kKeyPins[0], kKeyPins[1], kKeyPins[2], kKeyPins[3], !(lv & 1), !(lv & 2), !(lv & 4), !(lv & 8), a.input_.stuckMask());
        return true;
      }
      case 3: {
        char s[40] = "";
        for (uint8_t k = 0; k < a.n0_; ++k) { char t[5]; snprintf(t, sizeof t, " %02X", a.scan0_[k]); strncat(s, t, sizeof s - strlen(s) - 1); }
        snprintf(b, n, "i2c0 SDA8 SCL7 100k  scan:%s", a.n0_ ? s : " -");
        return true;
      }
      case 4: {
        char s[24] = "";
        for (uint8_t k = 0; k < a.n1_; ++k) { char t[5]; snprintf(t, sizeof t, " %02X", a.scan1_[k]); strncat(s, t, sizeof s - strlen(s) - 1); }
        snprintf(b, n, "i2c1 SDA10 SCL11 400k  scan:%s", a.n1_ ? s : " -");
        return true;
      }
      case 5: snprintf(b, n, "oled SSD1306 %s", a.p_.display && a.p_.display->ok() ? "OK" : "E-OLED-NONE (headless)"); return true;
      case 6:
        if (v.thermo.chip == ShtChip::Sht4x || v.thermo.chip == ShtChip::Sht3x)
          snprintf(b, n, "temp %s @%02X sn=%08lX OK", v.thermo.chip == ShtChip::Sht4x ? "SHT4x" : "SHT3x", v.thermo.addr, (unsigned long)v.thermo.serial);
        else snprintf(b, n, "temp %s", a.thermo_.probing() ? "probing" : errInfo(v.thermo.err).mnemonic);
        return true;
      case 7:
        if (v.mag.chip == MagChip::None || v.mag.chip == MagChip::Unknown) snprintf(b, n, "mag  %s", errInfo(v.mag.err).mnemonic);
        else snprintf(b, n, "mag  %s @%02X id=%02X %s  cal=%s %d%%  mount=%d decl=%+d", v.mag.chip == MagChip::Qmc5883l ? "QMC5883L"
                      : v.mag.chip == MagChip::Qmc5883p ? "QMC5883P" : "HMC5883L", v.mag.addr, v.mag.idByte, v.mag.idWarning ? "W-MAG-ID" : "OK",
                      a.compass_.calibration().valid ? "YES" : "NONE", a.compass_.calibration().qualityPct,
                      a.settings_.get(SettingId::Mount), a.settings_.get(SettingId::Declination));
        return true;
      case 8:
        if (v.bat.present) snprintf(b, n, "bat  %u mV %u%%", v.bat.mv, v.bat.pct);
        else snprintf(b, n, "bat  not fitted (kPinBatteryAdc = -1)");
        return true;
      case 9: {
        char t[100];
        timeLine(a, t, sizeof t, false);
        snprintf(b, n, "time %s%s", t, a.time_.meta().q == ClockQuality::Approx || a.time_.meta().q == ClockQuality::None
                 ? "  -> run tools/sync_time.py or send \"TIME <unix>\"" : "");
        return true;
      }
      case 10: {
        char scr[12], bri[8], heat[16];
        FormatCtx c = v.fmt;
        formatSettingValue(SettingId::ScreenOff, a.settings_.get(SettingId::ScreenOff), ValueStyle::List, c, scr, sizeof scr);
        formatSettingValue(SettingId::Brightness, a.settings_.get(SettingId::Brightness), ValueStyle::List, c, bri, sizeof bri);
        formatSettingValue(SettingId::Heater, a.settings_.get(SettingId::Heater), ValueStyle::List, c, heat, sizeof heat);
        snprintf(b, n, "cfg  ver1 %s tz=%s scr=%s bri=%s heat=%s", a.persist_.ok() ? "OK" : "E-NVS(RAM only)", a.tz().id, scr, bri, heat);
        return true;
      }
      case 11: {
        char f[80] = "";
        for (int d = 0; d < (int)Device::Count; ++d) {
          const DeviceHealth& h = a.health_.get((Device)d);
          if (h.code == ErrCode::None || errInfo(h.code).sev < Severity::Warn || h.h == Health::Ok) continue;
          strncat(f, " ", sizeof f - strlen(f) - 1);
          strncat(f, errInfo(h.code).mnemonic, sizeof f - strlen(f) - 1);
        }
        snprintf(b, n, "faults:%s", f[0] ? f : " none");
        return true;
      }
      case 12: snprintf(b, n, "=== ready ==="); return true;
      default: return false;
    }
  }

  static bool help(uint16_t i, char* b, size_t n) {
    static const char* const k[] = {
        "PING | HELP | INFO", "TIME <unix>[.ms] | TIME YYYY-MM-DD HH:MM[:SS] | TIME? (T)", "TZ <id> | TZ?",
        "SCAN | SHT | HEAT | MAG <s> | KEYS", "GET <key> | SET <key> <value>   keys: tz scr bri anim aoff lslp tunit toff heat hpw decl mnt ndl",
        "LOG E|W|I|D | LOG DUMP | TLM <s> (0 = off)", "REBOOT | FACTORY YES"};
    if (i >= sizeof k / sizeof k[0]) return false;
    snprintf(b, n, "%s", k[i]);
    return true;
  }

  static bool gen(void* ctx, uint16_t i, char* b, size_t n) {
    AppCore& a = *static_cast<AppCore*>(ctx);
    switch (mode) {
      case Long::Boot: return bootLine(a, i, b, n);
      case Long::Help: return help(i, b, n);
      case Long::Zones:
        if (i >= kTzCount) return false;
        snprintf(b, n, "%s  %-5s %-14s %s", kTzTable[i].id, kTzTable[i].stdAbbr, kTzTable[i].title, kTzTable[i].note);
        return true;
      case Long::Dump: {
        const LogRing& r = a.log_.ring();
        if (i == 0) { ringStart = 0; snprintf(b, n, "--- log: %u lines ---", r.count()); return true; }
        if (i - 1 >= r.count()) return false;
        snprintf(b, n, "%s", r.line((uint8_t)(i - 1)));
        return true;
      }
      case Long::Replay:
      case Long::Info: {
        if (bootLine(a, i, b, n)) return true;
        const uint16_t j = (uint16_t)(i - 13);
        if (mode == Long::Info) {
          if (j == 0) {
            snprintf(b, n, "state sw=%s %lld ms  tmr=%s %lld s  page=%u  display=%s  usb=%s", a.sw_.state() == Stopwatch::State::Running ? "RUN" :
                     a.sw_.state() == Stopwatch::State::Paused ? "PAUSE" : "IDLE", (long long)(a.vm_.sw.elapsedUs / 1000),
                     a.tm_.state() == Countdown::State::Running ? "RUN" : a.tm_.state() == Countdown::State::Paused ? "PAUSE"
                     : a.tm_.state() == Countdown::State::Done ? "DONE" : "SELECT", (long long)(a.vm_.tm.remainingUs / 1000000),
                     a.ui_.state().page, a.power_.displayOn() ? "on" : "off", a.power_.usbHost() ? "host" : "none");
            return true;
          }
          return false;
        }
        const LogRing& r = a.log_.ring();
        if (j == 0) { snprintf(b, n, "--- log: %u lines ---", r.count()); return true; }
        if (j - 1 >= r.count()) return false;
        snprintf(b, n, "%s", r.line((uint8_t)(j - 1)));
        return true;
      }
    }
    return false;
  }

  static void start(AppCore& a, Long m) {
    if (a.log_.longActive()) { a.log_.reply("ERR BUSY a long output is still printing, try again"); return; }
    mode = m;
    app = &a;
    a.log_.startLong(gen, &a);
  }

  static void telemetry(AppCore& a) {
    const ViewModel& v = a.vm_;
    a.log_.reply("TLM t=%.2f rh=%.1f dp=%.1f hdg=%05.1f Bh=%.1fuT raw=%d,%d,%d sw=%s %lld tmr=%s %lld pwr=%s page=%u i2c=%lu/%lu loop=%.1fms",
                 (double)v.thermo.tC, (double)v.thermo.rh, (double)v.thermo.dewC, (double)v.mag.headingDeg, (double)v.mag.fieldUt,
                 v.mag.raw[0], v.mag.raw[1], v.mag.raw[2], v.sw.st == Stopwatch::State::Running ? "RUN" : "STOP",
                 (long long)(v.sw.elapsedUs / 10000), v.tm.st == Countdown::State::Running ? "RUN" : "STOP",
                 (long long)Countdown::displaySeconds(v.tm.remainingUs), a.power_.displayOn() ? "ON" : "OFF", a.ui_.state().page,
                 (unsigned long)v.sys.bus0.fail, (unsigned long)v.sys.bus1.fail, (double)(a.loopMaxUs_ / 1000.0));
  }

  static void err(AppCore& a, const char* why, MonoUs now) {
    a.log_.reply("ERR %s", why);
    char t[16];
    size_t i = 0, spaces = 0;
    for (; why[i] && i < sizeof t - 1; ++i) { if (why[i] == ' ' && ++spaces == 2) break; t[i] = why[i]; }
    t[i] = 0;
    a.ui_.toast(S_T_CMD_ERR, t, now);
    a.log_.log(LogLevel::W, "CMD", "rejected: %s", why);
  }

  static void syncReply(AppCore& a, SyncVerdict v, int32_t d, MonoUs now) {
    if (v != SyncVerdict::Ok) {
      const char* why = v == SyncVerdict::BeforeMin ? "TIME BEFORE_MIN" : v == SyncVerdict::AfterMax ? "TIME AFTER_MAX" : "TIME BEFORE_BUILD";
      a.log_.reply("ERR %s", why);
      a.ui_.toast(S_T_SYNC_REJ, "PC clock?", now);
      a.health_.set(Device::Clock, Health::Degraded, ErrCode::SyncRejected, now);
      a.log_.log(LogLevel::W, "CLK", "W-SYNC-REJ %s", why);
      return;
    }
    char t[110];
    timeLine(a, t, sizeof t, false);
    a.log_.reply("OK TIME %s delta=%+.2fs", t, d / 1000.0);
    char ds[16];
    snprintf(ds, sizeof ds, "%+.1f s", d / 1000.0);
    a.ui_.toast(S_T_TIME_SET_D, ds, now);
    a.health_.set(Device::Clock, Health::Ok, ErrCode::None, now);
    a.log_.log(LogLevel::I, "CLK", "synced, delta %+ld ms", (long)d);
    ++a.modelRev_;
  }

  static void exec(AppCore& a, const CmdLine& c) {
    const MonoUs now = a.p_.mono->now();
    switch (c.cmd) {
      case Cmd::None: return;
      case Cmd::Error: err(a, c.err ? c.err : "UNKNOWN", now); return;
      case Cmd::Ping: a.log_.reply("PONG HG2 %s %lld", kFwVersion, (long long)((now - a.bootAt_) / 1000)); return;
      case Cmd::Help: start(a, Long::Help); return;
      case Cmd::Info: start(a, Long::Info); return;
      case Cmd::TimeUnix: { int32_t d = 0; syncReply(a, a.time_.syncUnixMs(c.unixMs, &d), d, now); return; }
      case Cmd::TimeLocal: { int32_t d = 0; syncReply(a, a.time_.setLocal(c.local, a.tz(), ClockSource::Serial, &d), d, now); return; }
      case Cmd::TimeQuery: { char t[120]; timeLine(a, t, sizeof t, true); a.log_.reply("%s zone=%s", t, a.tz().id); return; }
      case Cmd::TzSet: {
        const int z = findTz(c.key);
        if (z < 0) { err(a, "TZ UNKNOWN zone id, send 'TZ?' for the list", now); return; }
        if (a.saveSetting(SettingId::TimeZone, (int16_t)z, now)) {
          a.log_.reply("OK TZ %s", kTzTable[z].id);
          a.ui_.toast(S_T_ZONE, tzAbbr(kTzTable[z], a.time_.utcS()), now);
        } else {
          a.log_.reply("ERR NVS zone applied for this session, not stored");
        }
        return;
      }
      case Cmd::TzQuery: start(a, Long::Zones); return;
      case Cmd::Scan: {
        a.scanBuses();
        char s[64] = "";
        for (uint8_t k = 0; k < a.n0_; ++k) { char t[5]; snprintf(t, sizeof t, " %02X", a.scan0_[k]); strncat(s, t, sizeof s - strlen(s) - 1); }
        a.log_.reply("OK SCAN bus0:%s", a.n0_ ? s : " -");
        s[0] = 0;
        for (uint8_t k = 0; k < a.n1_; ++k) { char t[5]; snprintf(t, sizeof t, " %02X", a.scan1_[k]); strncat(s, t, sizeof s - strlen(s) - 1); }
        a.log_.reply("OK SCAN bus1:%s", a.n1_ ? s : " -");
        return;
      }
      case Cmd::Sht:
        a.thermo_.measureSoon();
        a.log_.reply("OK SHT chip=%s t=%.2f rh=%.2f crcErr=%u ioErr=%u", a.thermo_.chip() == ShtChip::Sht4x ? "SHT4x"
                     : a.thermo_.chip() == ShtChip::Sht3x ? "SHT3x" : "none", (double)a.thermo_.lastRawT(), (double)a.thermo_.lastRawRh(),
                     a.thermo_.crcErrors(), a.thermo_.ioErrors());
        return;
      case Cmd::Heat: {
        HeaterDecision why;
        if (a.thermo_.heaterTest(now, a.thermoCtx(false), &why)) a.log_.reply("OK HEAT pulse started, 'pre/hot' follows in the log");
        else err(a, why == HeaterDecision::SkipHot ? "HEAT HOT above 60 C" : why == HeaterDecision::SkipSpacing ? "HEAT SPACING wait 60 s"
                    : why == HeaterDecision::SkipUnsupported ? "HEAT UNSUPPORTED SHT4x only" : "HEAT BUSY", now);
        return;
      }
      case Cmd::Mag:
        a.magStreamUntil_ = now + (int64_t)c.num * kUsPerS;
        a.log_.reply("OK MAG streaming %d s: ms,x,y,z,ovf", (int)c.num);
        return;
      case Cmd::Keys: {
        const uint8_t lv = a.p_.keys ? a.p_.keys->rawPressedMask() : 0;
        a.log_.reply("KEYS pressed(1=down) ^=%d v=%d #=%d *=%d  pins %u %u %u %u  stuck %02X", lv & 1, (lv >> 1) & 1, (lv >> 2) & 1,
                     (lv >> 3) & 1, kKeyPins[0], kKeyPins[1], kKeyPins[2], kKeyPins[3], a.input_.stuckMask());
        return;
      }
      case Cmd::Get: {
        const int i = findSettingByKey(c.key);
        if (i < 0) { err(a, "GET KEY unknown key, see HELP", now); return; }
        char t[32];
        formatSettingValue((SettingId)i, a.settings_.get((SettingId)i), ValueStyle::List, a.vm_.fmt, t, sizeof t);
        for (char* p = t; *p; ++p) if (*p == '`') *p = 'o';
        a.log_.reply("%s=%d (%s)", c.key, a.settings_.get((SettingId)i), t);
        return;
      }
      case Cmd::Set: {
        const int i = findSettingByKey(c.key);
        if (i < 0) { err(a, "SET KEY unknown key, see HELP", now); return; }
        const SettingId id = (SettingId)i;
        int16_t v;
        if (id == SettingId::TimeZone) {
          v = (int16_t)findTz(c.val);
        } else {
          char* end = nullptr;
          const long n = strtol(c.val, &end, 10);
          v = end && *end == 0 ? (int16_t)n : (int16_t)-32768;
        }
        if (!isValid(id, v)) { err(a, "SET RANGE value out of range", now); return; }
        if (a.saveSetting(id, v, now)) { a.log_.reply("OK %s=%d", c.key, v); a.ui_.toast(S_T_SAVED, nullptr, now); }
        else a.log_.reply("ERR NVS applied for this session, not stored");
        return;
      }
      case Cmd::Log: {
        const LogLevel l = c.key[0] == 'E' ? LogLevel::E : c.key[0] == 'W' ? LogLevel::W : c.key[0] == 'D' ? LogLevel::D : LogLevel::I;
        a.log_.setLevel(l);
        a.log_.reply("OK LOG %c", c.key[0]);
        return;
      }
      case Cmd::LogDump: start(a, Long::Dump); return;
      case Cmd::Tlm: a.tlmS_ = (uint32_t)c.num; a.log_.reply("OK TLM %d", (int)c.num); return;
      case Cmd::Reboot: a.log_.reply("OK REBOOT"); a.snapshot(now); a.p_.power->restart(); return;
      case Cmd::Factory: a.log_.reply("OK FACTORY erasing hg2 and restarting"); a.persist_.factory(); a.p_.power->restart(); return;
    }
  }
};
ConsoleAccess::Long ConsoleAccess::mode = ConsoleAccess::Long::Boot;
AppCore* ConsoleAccess::app = nullptr;
uint16_t ConsoleAccess::ringStart = 0;

void logStartBootReport(AppCore& app) { ConsoleAccess::start(app, ConsoleAccess::Long::Boot); }
void logStartReplay(AppCore& app) { ConsoleAccess::start(app, ConsoleAccess::Long::Replay); }
void logTelemetry(AppCore& app) { ConsoleAccess::telemetry(app); }
void AppCore::execute(const CmdLine& c) { ConsoleAccess::exec(*this, c); }

}  // namespace hg
