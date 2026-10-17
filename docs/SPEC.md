# Hiking Gadget v2: Final Specification

Status: **implementation-ready**, written 2026-10-11, revised the same day after review round 2 (rulings D24–D37 in §0.3). This document is the contract for every module. Where it conflicts with an older candidate spec, this document wins. Where it conflicts with `docs/HW_FACTS.md` on a hardware fact, HW_FACTS wins (BRIEF §6a), and the conflict is a spec bug to report.

Inputs: `docs/BRIEF.md` (authoritative brief), `docs/HW_FACTS.md` (211 hardware facts), three candidate specs (A: UX and visual craft, B: embedded robustness, C: simplicity and testability), and three judges' verdicts.

---

## 0. How to read this spec

### 0.1 Base and merge

| Part | Taken from | Why |
|---|---|---|
| Skeleton: module tree, single-threaded loop with no key ISRs, pure decision functions (`decideBootClock`, `decidePower`, `heaterDue`, `parseLine`), one NVS key per setting, implementation order with a gate per step | **C** | Two of three judges named C the base: the simplest correct runtime and the most implementable plan. |
| Interaction model and visual design: soft-key rail next to the physical keys, one `Bindings` table for hints, dispatch and docs, trigger grammar, hold-fill, typography, Home/Timer/Compass designs, modal settings list, level-only calibration, background chips | **A** | All three judges rated A best on UX and hardware fit. |
| Robustness layer: strong time types, debounced USB detection, sleep error handling, `rtc_gpio_hold_dis`, stuck-key exclusion, boot-loop guard, health registry with error codes, NVS clock checkpoint, Web Serial sync page, tile-level frame diff, safe slide composer, layout audit, provenance tags | **B** | B carried the platform-verified details. |
| Legibility floor (≥ 7 px for text a user must read), `__DATE__/__TIME__` gated by a build-ID hash, RH correction for the temperature offset, keymap test "no icon = no action" | **C** | Judges' must-fix items. |

### 0.2 Provenance tags

The user's rule: a statement is "checked" only if it was checked. Every load-bearing fact carries one tag.

| Tag | Meaning |
|---|---|
| **[V-build]** | Compiled in this session with arduino-cli, core **3.3.12**, FQBN `esp32:esp32:esp32s3:CDCOnBoot=cdc`. Probe sketch: `/mnt/wd/arduino-esp32/tmp/spec_final/api_probe/api_probe.ino`. It compiled with `--warnings all` and no warnings: 357,010 B flash, 25,400 B RAM. |
| **[V-src]** | Read in this session from the installed sources: core 3.3.12 (`HWCDC.cpp`, `Wire.cpp`, `esp32-hal-i2c-ng.c`, `boards.txt`, `platform.txt`), the esp32s3-libs headers and `sdkconfig`, and U8g2 **2.36.19** `clib`. |
| **[V-render]** | Rendered in this session on the host with the **installed U8g2 2.36.19** clib (`/dev/shm/arduino-esp32/user/libraries/U8g2/src/clib`). Prototype: `/mnt/wd/arduino-esp32/tmp/spec_final/proto/`. Contact sheet: `.../proto/contact_sheet.png` (53 states). Per-state 4× PNGs: `.../proto/out/`. The prototype predates review round 2: where it differs from §3.8, §3.8 wins. |
| **[V-meas]** | Review round 2: string widths and ink extents measured with the installed U8g2 2.36.19 (`u8g2_GetStrWidth` + ink scan; tool `/mnt/wd/arduino-esp32/tmp/spec_fix/meas.cpp`). Positions derived from them arithmetically. **Not re-rendered**; the WP8/WP9 renders and the layout audit are the check. |
| **[HWF x]** | Fact `x` in `docs/HW_FACTS.md`. Its status there is SINGLE-SOURCE unless noted VERIFIED. |
| **[DS]** | Datasheet value carried over from HW_FACTS or a candidate spec. Not re-read in this session. |
| **[EST]** | Estimate. Not measured. |
| **[HW?]** | Unknown until the real board is tested. Each one maps to a step in §11 or a row in §12. |

### 0.3 Decisions (judge conflicts and my rulings)

| # | Topic | Options on the table | Decision | Reason |
|---|---|---|---|---|
| D1 | Base | A (judge 1) vs C (judges 2 and 3) | C skeleton + A UX + B robustness | Majority. Judge 1's must-fix list is fully satisfied through the grafts. |
| D2 | Toolchain | A designed for 2.0.9 / C++11 | Core **3.3.12 only** (IDF 5.5.5). Code is C++17 (host `-std=c++17`, device effective `-std=gnu++2a` with `-fno-rtti` [V-src `flags/cpp_flags`]). `#error` if `ESP_ARDUINO_VERSION_MAJOR < 3`, in `hal/build_checks.h` only (D35). | Brief, disk and all judges agree. |
| D3 | Clock model | A: `gettimeofday` never set ("chrono"). B/C: esp_timer for intervals, settable wall clock. | **Three strong types.** `MonoUs` (esp_timer) for every interval while awake. `UtcMs` (system time, set only in `hal_clock.cpp`) for display. `RtcUs` (`esp_rtc_get_time_us()`) **only** to carry model snapshots across deep sleep and software resets. | IDF re-anchors `gettimeofday` to the RTC counter after every light sleep (`esp_set_time_from_rtc`, [HWF esp32s3-power F21]). An A-style chrono would therefore step at each wake. Stopwatch and timer stay XTAL-accurate while awake and survive deep sleep through an explicit, unit-tested bridge (§5.4). This meets judge 1's R6 goal and judges 2/3's strong-type requirement. |
| D4 | Models across power-off | A: survive deep sleep. C: deep sleep blocked while busy. | Survive deep sleep, SW reset, panic and WDT via `RTC_NOINIT` snapshot + `RtcUs` bridge. Lost only on power loss. Timer arms an RTC wake. | R6 ("nothing resets"). The bridge is a pure function. |
| D5 | Clock after power loss | C: "CLOCK NOT SET". B/A: NVS checkpoint as APPROX. | NVS checkpoint → **Approx**, plus build stamp gated by build ID (newest wins). `--:--` only when neither exists. | Judge 2 must-fix. Judge 1 wants APPROX until synced. A stale-but-flagged date beats no date on a trail. |
| D6 | Settings list | C: `#` = down only | Modal list. `^`/`v` move the cursor with repeat; the rail shows chevrons. `#` = back, `*` = edit. | All judges. |
| D7 | Hidden actions | A: Compass hold-`#` with no cap | Zero hidden actions, including hold actions, enforced by `test_keymap`. Compass `#` = calibrate (cap always drawn). | All judges. |
| D8 | Destructive actions | A: hold. B/C: single press. | Hold 0.8 s with visible fill: stopwatch reset, timer cancel, compass target clear. Hold 1.5 s: power off, reset settings, reboot. A short press shows a toast such as "Hold # to reset". | Judge 1 must-fix. |
| D9 | Stopwatch/timer key map | A: lap on click, reset while running | Stopwatch: lap fires on **Down** (instant). Reset only when **paused**. Timer: `+1 min` only while running. Cancel only when paused (hold). | Removes long actions from running states, so every running-state action fires on key-down with zero latency. |
| D10 | Light sleep | A: between frames with display on | **Only while the display is off**, with ≥ 50 ms to the next deadline, no key down, and no USB host. | Judges 2/3. Wake overhead is about 8.6 ms [HWF F20]. Sleeps under about 10 ms are rejected. |
| D11 | Display flush | A: raw 0x21/0x22 flush | Keep U8g2's path: shadow-buffer diff, then `u8g2_UpdateDisplayArea` per dirty page span, at most 2 pages per loop iteration. | SH1106/0x3D fallback stays possible. The loop never blocks more than about 10 ms. |
| D12 | Calibration | B: "tilt & turn" | **Level only**, two slow turns. Samples are rejected when the Z deviation shows tilt (§6.4). | Physics: inclination about 72° [HWF hmc F17/F23]. |
| D13 | QMC5883P `0x29=0x06` | J2: on (datasheet). J3: off until verified. | **On by default** (`kQmc5883pWriteSignReg = true`). | The QST datasheet writes it in every setup example [HWF P-REG29]. BRIEF §6a: take QMC5883P values from the datasheet. |
| D14 | Deep-sleep RTC_PERIPH | Brief: optional. Judges: ON. | ON (`kDeepSleepKeepRtcPeriph = true`). | About +1 µA [HWF F12]. Deterministic pull-ups. Hardware test §11 step 14. |
| D15 | Heater | C: fixed intervals silently gated by RH ≥ 75 % | Fixed intervals always pulse. **Auto** pulses when RH ≥ 90 % on two readings, at most every 10 min. Every skip is visible (picker sub-label, Home details, log). SHT3x: "N/A" (sub-label `SHT3x has no pulse heater`). | Principle "a silent program is a defect". 90 % is Sensirion's creep threshold [HWF sht4x F09]. |
| D16 | Fonts | A: 3×5 micro caps for user text | Legibility floor: anything the user must read (date, units, errors, hints, keycap labels) is ≥ 7 px (`haxrcorp4089` or `helvB08`). The 5 px micro font is only for status-line decoration, tags and Diagnostics. | Judges 1 and 3. |
| D17 | Scope cuts | B's wizard, polarity auto-detect, pull-up probe, chords, hold-to-wake, 12 h clock, date formats, custom timer slot, custom TZ, `hg_sim`. A's CRT, splash animation, list glide/bump, picker wheel, chip glide, lap shift, ring pop. | **Cut from v2.0.** Kept motion: page slide, compass spring, alert invert, keycap press/hold-fill, toast slide, contrast fade. | Judge 3 must-fix. All are listed in §1.3 as v2.1 candidates. |
| D18 | Sync tools | J1: ship all three. J3: Web Serial at P2. | Ship the typed command, `tools/sync_time.py` and `tools/sync/hg_sync.html`. The HTML page is the last, independent work package (WP13). | The user is on Windows; the HTML page needs no Python. It is small and does not block firmware. |
| D19 | Key debounce | C: single sample | Press accepted after ≥ 15 ms LOW **and** ≥ 2 samples. `tDown` = first LOW sample. | Judge 3. Timing accuracy is kept. |
| D20 | Rail key rows | A: ^ −19, v 16, # 48, * 80. J2: ^ −9, v 20, # 45, * 68. | A's values. | My own measurement of the listing photo (§2.1) gives ^ ≈ −17…−22, v ≈ 16, # ≈ 49…52, * ≈ 82…87, agreeing with judge 1. Theme constants, [HW?]. |
| D21 | Settings storage | A: CRC blob. C: key per setting. | One NVS key per setting + `ver`. Calibration and clock are CRC blobs. | Judge 2. Simple migration, per-key validation. |
| D22 | Error codes | A: mnemonic. B: numeric. | Mnemonic (`E-SHT-NONE`). The README explains each in Russian. | Self-explanatory on screen. |
| D23 | UI language | All specs: English | English, with the evidence put to the user as **Open question Q1** (§1.4). All strings in `ui/strings*.h`. | Judge 1 must-fix: make it the user's call. |
| D24 | WP0 contracts (review 2, blocker) | §4.4 as first written did not compile and left files without owners | `FormatCtx`/`ScreenId` move to WP0's `ui/view_types.h`; `BatteryState`, `ResetReason`, `WakeCause` to `core/types.h`; `errTitle/errHint` declared in `ui/strings.h`; `screen.h` forward-declares `ViewModel`/`UiState`; `ports.h` only includes `ui/display_port.h`; screen registry `ui/screens/registry.h`; ownership of `u8g2_inc.h`, `i2c_bus.cpp`, empty `strings_*.h` and registry stubs given to WP0 (§13). | 13 packages start in parallel against these headers; they must compile as written. |
| D25 | Key press latching (review 2) | Bindings re-read every iteration | `InputLogic` latches the `KeyConfig` at Down and `UiController` latches the `Binding`; a change of bindings affects only the next press. An OnDown press never produces Click or Long. **Modified from the review**: it still produces `Repeat` when the latched binding has `repeat` (lists and pickers fire on Down and auto-repeat; "only Up" would break scrolling). | Stops one press from triggering two actions (mark target then clear it; open list then reset settings). |
| D26 | Stuck key while awake (review 2) | KEY STUCK evaluated only before deep sleep | After `kKeyStuckMs` = 10 s LOW a key is ignored for gestures, timeout, light sleep and wake, with a card now; it recovers after 1 s HIGH (§2.2). | A shorted key must not keep the display on and drain the battery silently. |
| D27 | Phantom wakes (review 2) | Any ext1 wake with no key LOW at boot counted | Only short-sleep storms count: no key LOW **and** the previous deep sleep < 3 s; exclusion after 5 within 2 min (§7.5). | A brisk tap is often released before the boot sample [EST]. |
| D28 | USB detection while dark (review 2) | Light sleep gates the USB pads, so a PC plugged in while the screen is off was never seen | No light sleep while the raw USB signal or its debounce is pending; every 8th light-sleep slice stays awake 300 ms to sample it (§7.2). Tools and README say "press any gadget key first" as the fast path. | Cost ≈ +0.25 mA while dark [EST], against a sync that otherwise never works without a key press. |
| D29 | UI strings (review 2) | `·`, `−`, `°` in strings; toasts up to 146 px | ASCII only. The backtick byte 0x60 (`kDegChar`) is drawn as the small degree by `Canvas::text/num`; in this spec a `°` inside a UI string means that byte. Toast text ≤ 105 px. `test_strings` enforces both. | The `_tr` fonts have no such glyphs and the toast box is 117 px [V-meas]. |
| D30 | Top-line budget (review 2) | No rule; date, chips, `SYNC?` and zone overlapped | Fixed priority ⚠ > bell > timer > stopwatch > `SYNC?`; a page never shows its own chip; on overflow Home first drops the weekday, then folds `SYNC?` into ⚠, then drops the lowest chips (§3.4). | Approx clock + timer + stopwatch is a normal hike state. |
| D31 | Serial output (review 2) | 256 B HWCDC ring, `write()` ignored | `setTxBufferSize(4096)` before `begin()`; `ISerialPort::write` returns the accepted count; `Logger` paces long outputs by `availableForWrite()` and reports dropped bytes (§7.3). | Boot report, `INFO`, `LOG DUMP` were cut to about 256 B [V-src HWCDC.cpp]. |
| D32 | State lost across deep sleep (review 2) | Silent defaults | `E-RTC-LOST` card ("STATE LOST" / "TIMER LOST"), display on, logged (§5.4). | The user's alarm must not vanish silently. |
| D33 | SHT probe time (review 2) | 0x89 busy-retry up to 1.2 s on every probe | 0x89 is retried once (10 ms). **Modified from the review**: the 1.2 s wait is kept, but only when `RtcState.shtBusyUntil` says a heater pulse was running at the reset; on DEEPSLEEP wakes the cached chip is verified with one measurement. | HWF sht4x notes: whether a busy SHT4x accepts 0x94 is undocumented, so "0x94 already aborts the pulse" is not proven. |
| D34 | Status register broken ("DA 5883") (review 2) | HMC path waited for RDY forever | At 0x1E, RDY stuck at 0 for 500 ms while the data changes → timed reads every 1/ODR + `W-MAG-NOSTATUS` (§6.3). | [HWF Q32]. |
| D35 | Build guards (review 2) | `#warning` for CDC; version `#error` in `config/` | `hal/build_checks.h` (first include of every `hal/*.cpp`) holds `#error`s for the core version and for USB CDC On Boot / USB Mode. | `-w` (IDE default "Compiler warnings: None") hides `#warning`; `config/` is included by host code [V-meas g++]. |
| D36 | Reset reasons (review 2) | Review proposed `ResetReason` 0..12 | Mirrors all 16 `esp_reset_reason_t` values of IDF 5.5 (adds `Efuse`, `PwrGlitch`, `CpuLockup`), checked by `static_assert` in `hal/hal_power.cpp`. `Sw` and `Usb` are non-crash (§5.4). | [V-src `esp_system.h`]. |
| D37 | Prototype as acceptance reference (review 2) | "Match the prototype positions"; copy PBMs into the repo | The review's second option: the clause is dropped (no PBMs copied into the repo). Acceptance is §3.8 + layout audit + human review of the contact sheet. | The prototype lives in scratch outside the repo and is outdated after round 2 (this reviser may edit only SPEC.md). |

### 0.4 Open question for the user (answer before WP8/WP9 start; English is the default if no answer)

**Q1. Should the on-screen language be English or Russian?** The evidence is in §1.4. In short: the only modern-looking small Cyrillic font lacks «э», «Ё» and «ё» [V-render]. Russian labels are 30-60 % longer on a 117-px content width. Not only the Body and Micro text changes: the `Label` and `Value` fonts (card titles, TIME UP, CALIBRATED, cardinals, picker values) have no Cyrillic either and need replacement faces that are taller, monospaced and less refined (§1.4 table). A Russian pack is therefore 4 font constants + strings + a re-tune of every Label/Value layout, estimated at 2–3 days [EST].

---

## 1. Scope

### 1.1 Requirement traceability

| Req (BRIEF §1) | Feature(s) | Spec sections | Acceptance |
|---|---|---|---|
| R1 Carousel of 5 screens | Home → Stopwatch → Timer → Compass → Settings. Vertical slide (keys are vertical), wrap-around, page dots in the rail between `^` and `v`. | §2.4, §3.6, §3.8 | `test_ui_nav`; renders `slide_*` |
| R2 Two arrows page, two keys act | `^`/`v` page on every carousel page. `#`/`*` act per screen, with keycaps drawn next to the physical keys from the same `Bindings` that dispatch. | §2.2–2.4 | `test_keymap`, `BUTTONS.md` |
| R3 Home: time from PC (MSK default, zone in settings), date, T, RH, battery stub | Hero clock + seconds, date, zone label, clock-quality tag, T/RH/dew point, battery glyph "not fitted", details card. Time from build stamp, Serial sync (script, web page, typed), manual set. | §3.8.1, §5 | `test_clock_policy`, `test_tz`, Home renders |
| R4 Stopwatch start/stop + lap | `*` start/pause/resume (key-down timestamp), `#` lap (key-down), hold `#` reset (paused). 99 stored laps, last two shown with delta, all logged to Serial. | §2.5, §3.8.2 | `test_stopwatch` |
| R5 Timer presets 1/3/5/10/30/60 | `#` next preset, `*` start/pause/resume, `#` +1 min while running, hold `#` cancel while paused. Alert on any screen and from any power state; overtime; restart same preset. | §2.5, §3.8.3 | `test_countdown`, `test_app_scenarios` |
| R6 Values persist across screens | Models live in `AppCore`, tick every loop on every page and power state. Background chips on every page. Snapshots survive deep sleep, SW reset and panic. | §4.2, §5.4, §7 | `test_rtc_bridge`, scenario "stopwatch through power-off" |
| R7 Compass, circle with cardinals, smooth arrow | Rotating card with fixed lubber line, 15° ticks, N disc, needle, critically damped spring, 16-point name, TRUE/MAG, target bearing with steer, calibration, interference and saturation tags. | §3.8.4, §6.4 | `test_compass_math`, `test_mag_calibrator` |
| R8 Settings: popular zones, SHT heater interval, more | 19 rows in 6 sections (§8): 18 zones with DST rules, heater interval and power, temperature unit and offset, declination, mount, needle speed, brightness, screen and auto-off timeouts, light sleep, calibrate, diagnostics, power off, reset, version. | §8 | `test_settings` |
| R9 Screen off for battery | Dim → off at the timeout, `*` on Home = off now, auto power-off (deep sleep), light sleep while dark. | §7 | `test_power_policy` |
| R10 Modern hi-tech minimal UI | Design system §3: 9 fonts, grid, rail, dotted hairlines, one hero number per page, motion spec. | §3 | Golden PNGs, layout audit |
| R11 Arduino IDE | `.ino` at the root with `setup()`/`loop()` only; everything under `src/`; only external library is U8g2. | §4.1, §10.6 | `tools/build.sh fw` |

### 1.2 Feature list (v2.0)

- **Home.** HH:MM + seconds, weekday and date, zone abbreviation (DST-aware), clock-quality tag `SYNC?`, T (°C/°F, 0.1), RH (%), dew point, heater glyph, battery placeholder, background chips. `#` toggles a Details card. `*` turns the display off now; hold `*` 1.5 s powers off.
- **Stopwatch.** MM:SS.cc → H:MM:SS.c → HH:MM:SS. 99 laps stored (numbering continues up to lap 999), last two shown with delta, every lap logged. Pause/resume. Hold-to-reset when paused.
- **Timer.** Six presets. Start, pause, resume, +1 min (running), hold-to-cancel (paused), "ends at", draining progress bar. Full-screen alert on any page and from any power state (deep sleep wakes itself). Overtime count-up, restart same preset, missed-alert bell chip.
- **Compass.** Rotating card, needle, lubber line, true/magnetic heading, 16-point name, target bearing with steer delta, calibration modal (coverage ring, quality), interference/saturation/uncalibrated tags, mount picker with live preview, declination, missing/unknown chip card.
- **Settings.** Summary cover + modal list with section headers, picker with live preview, set-time editor, hold-to-confirm destructive rows.
- **Diagnostics** (5 pages): Sensors (with heater test), I2C scan, Buttons (K1..K4 mapping + rail alignment), Mag raw, System.
- **Fault reporting.** Boot report on Serial; "CHECK HARDWARE" card on screen; per-screen fault states; toasts for transient events; health registry with error codes.
- **Serial console.** Time sync, zone, scan, sensor reads, heater test, raw magnetometer stream, keys, settings get/set, log level and dump, telemetry, reboot, factory reset.
- **Host tooling.** Render every state to PNG from the firmware's UI code, contact sheet, `BUTTONS.md` generation, unit tests, layout audit, keymap test, build script, sync tools.

### 1.3 Deliberately out of v2.0

| Out | Why |
|---|---|
| WiFi/NTP, captive portal | The brief says time comes from the computer. About 600 KB flash and 80–120 mA while active [EST]; credential UI; v1 pitfalls 1 and 8. Seam: an NTP module would only call `TimeService::setUtc()` with a new `ClockSource` value added then (v2.0 has no `Ntp` value). |
| BLE, OTA, GPS, barometer | No hardware for them / not requested. |
| Buzzer or vibration | No hardware. The alert is visual. The GPIO48 LED is kept off (§7.6). |
| Tilt compensation | No accelerometer. The UI says "Hold level"; the error is about 3° per 1° of tilt at Moscow [HWF hmc F23]. |
| Custom timer durations, custom timer slot | R5 asks for presets. `+1 min` covers "a bit longer". |
| Custom UTC offset, 12-hour clock, date formats | The brief asks for popular zones only. The user's locale is 24 h. Each would be one more settings row later. |
| Lap browser beyond the last two | Two rows fit legibly; every lap goes to Serial. |
| Button-mapping wizard, polarity auto-detect, chord gestures, hold-to-wake | Scope (D17). Mapping lives in `board_pins.h` and is verified with Diagnostics › Buttons. |
| Animated splash, CRT power-off, list glide/bump, picker wheel, chip glide, lap shift | Scope (D17). The `Motion` API (§3.6) takes them later without layout changes. |
| Left-handed (180°) mode | The silkscreen symbols would lie. Seam: content draws only in x 0..116, chrome draws the rail. |
| Battery measurement | No battery yet. `IBattery` stub + `kPinBatteryAdc` (§6.5). |
| History graphs, min/max logging | Not requested; would cost flash writes. |

### 1.4 UI language: English by default (Q1 to the user)

Evidence, measured in this session [V-render]:

1. `u8g2_font_haxrcorp4089_t_cyrillic`, the only small modern-looking face with Cyrillic, has no glyph for U+044D «э», U+0401 «Ё» or U+0451 «ё» (`u8g2_IsGlyph` = 0). U8g2 silently skips missing glyphs [HWF u8g2 U25], so "Таймаут экрана" would render as "Таймаут крана". «Й» exists. The other Cyrillic fonts are X11 terminal faces (`5x7_t_cyrillic`, `6x12_t_cyrillic`) and look dated.
2. None of the `_t_cyrillic` fonts contain the degree sign [HWF u8g2 U32]. Our degree is a primitive anyway.
3. Russian labels are 30–60 % longer than English on a 117-px content width; at 7 px type they need abbreviations.
4. Most of the UI is numbers and icons. English micro-copy (START, LAP, HOLD, OPEN) is standard on outdoor devices.

How the Russian-speaking user is still served:
- The README is in Russian with a glossary of every on-screen word and a table of every error code (`E-SHT-NONE` …) explaining what to check.
- Every visible string lives in `src/ui/strings*.h`; fonts are chosen by role in `theme.h`.
- **RU pack effort** (if Q1 = Russian). Every role that carries words needs a Cyrillic face. Measured with the installed U8g2 2.36.19 [V-meas]:

  | Role | English font (cap height) | Cyrillic candidate (cap height) | Effect on the layout |
  |---|---|---|---|
  | `Body` | `haxrcorp4089_tr` (7) | `haxrcorp4089_t_cyrillic` (7, lacks «э», «Ё», «ё») or `6x12_t_cyrillic` (7, all glyphs, monospace 6 px, about 10 % wider, dated look) | Russian words 30–60 % longer → rows need abbreviations |
  | `Micro` | `tom_thumb_4x6_mr` (5) | `4x6_t_cyrillic` (has «э/ё») | same height; status line and tags get longer |
  | `Label` | `helvB08_tr` (8) | `6x13B_t_cyrillic` (9, bold, monospace 6 px, «Д» descends 1 row) | +1 row per Label line; card header box 12 px is just enough; `ПРОВЕРЬТЕ ДАТЧИКИ` = 102 px of the 105 px header; cardinals С/В/Ю/З and timer chips re-centred |
  | `Value` | `helvB10_tr` (11) | `9x15_t_cyrillic` (10, regular weight, 9 px advance) or `6x13B_t_cyrillic` (9, bold) | picker values lose weight or height; `ЕКАТЕРИНБУРГ` = 108 px (9x15) → more Label fallbacks |

  Work: add `strings_ru.h`; switch the four font roles; replace «э/ё/Ё» if haxrcorp is kept; re-tune Label/Value positions; re-run the layout audit and sweeps. Estimate 2–3 days [EST].

---

## 2. Interaction model

### 2.1 Physical keys

From `oled-0.96-4buttons_listing.png`: four tact switches sit in a vertical column right of the glass, with silkscreen `^ v # *` from top to bottom. The header reads `GND VCC SCL SDA K4 K3 K2 K1`.

I measured the photo in this session: the marketing render places screen rows 0..63 at about 3.4 px per row. The key centres then fall at these screen rows: **`^` ≈ −17…−22 (above the glass), `v` ≈ 16, `#` ≈ 49…52, `*` ≈ 82…87 (below the glass)**. That agrees with spec A and judge 1. The key pitch is about 34 rows, so only `v` and `#` sit beside the active area. The rail therefore puts:
- the page indicator in the top-right corner, between `^` and `v`;
- the `#` keycap at rows 42..51;
- the `*` keycap flush at the bottom, rows 54..63 (as close to `*` as the glass allows).

These are theme constants, tuned on hardware with Diagnostics › Buttons (§11 step 4) [HW?].

| Physical | GPIO | Logical `Key` | Symbol | Role |
|---|---|---|---|---|
| K1 | 4 | `Key::Up` | `^` | previous page; up in lists and pickers |
| K2 | 5 | `Key::Down` | `v` | next page; down in lists and pickers |
| K3 | 6 | `Key::Hash` | `#` | secondary: lap, +1, next preset, back, cancel, details |
| K4 | 1 | `Key::Star` | `*` | primary: start/stop, mark, open, save, OK |

- The K→symbol mapping is the BRIEF's "most likely" one and is **unverified** [HW?]. It is defined in exactly one place, `kKeyPins[]` in `src/config/board_pins.h`.
- Inputs use `INPUT_PULLUP`, active LOW. GPIO1 is an ordinary RTC-capable GPIO [HWF supermini SM-05]; v1's "USB D−" claim is wrong.
- Ergonomics: the edge keys (`^`, `*`) are easiest to find blind. `*` therefore carries the most frequent action on every page. `#` sits next to `v`; a slip between them is harmless because paging never destroys state and destructive `#` actions need a hold.

### 2.2 Input grammar (pure `core/input`, host-tested)

| Parameter | Value (`timing.h`) | Note |
|---|---|---|
| Sampling | every loop iteration; the loop never blocks > ~10 ms (§4.5) | no GPIO interrupts on key pins |
| Press debounce | raw LOW for ≥ `kDebounceMs` = 15 ms **and** ≥ 2 consecutive samples | `tDown` = time of the **first** LOW sample of that run |
| Release debounce | raw HIGH for ≥ 15 ms and ≥ 2 samples | |
| Click | released < `kClickMaxMs` = 400 ms with no Long fired | |
| Hold visual | fill starts at `kHoldVisualMs` = 150 ms | taps never flicker the keycap |
| Long | `kHoldMs` = 800 ms (reset, cancel, clear target) | |
| Long, critical | `kHoldCriticalMs` = 1500 ms (power off, reset settings, reboot) | |
| Hesitation zone | release between 400 ms and the hold threshold → no action | protection against half-intended holds |
| Repeat | first after `kRepeatDelayMs` = 400 ms, then every 125 ms; after 1500 ms held every 50 ms | only bindings with `repeat = true` (lists, pickers, set-time) |
| Acceleration | step × 5 after 2000 ms of repeat | only bindings with `accel = true` (year, temp offset, declination) |
| Press latching | the `KeyConfig` (trigger, hold time, repeat, accel) is **latched at Down** for the whole press; `configure()` only affects the next press | see below (D25) |
| Multiple keys | the first key down owns the gesture; others are ignored until **all non-stuck** keys are up | except Diagnostics › Buttons (`captureAll`) |
| Stuck key | LOW continuously for `kKeyStuckMs` = 10 s → the key is **stuck**: it loses gesture ownership, emits nothing more, and leaves `downMask()`; it is no longer "a key held" for the screen timeout, light sleep, or wake (§7.2, §7.4, §7.5). It recovers after `kKeyStuckClearMs` = 1 s HIGH | not while `captureAll`; D26 |
| Chords | none | invisible, unreliable with one thumb |

**Trigger rule** (derived per binding, never written by hand):
- A binding with **no long action** fires its short action on **Down** (debounced press). Zero added latency: start/stop, lap, page change.
- A binding **with** a long action fires short on **Click** and long when the hold reaches its threshold.
- Timing actions always use `tDown`, so a stopwatch start or lap is stamped at the physical press even if dispatched later.

**Press latching** (D25). An action fired on Down often changes the screen state, and with it the binding of the same key: Compass `*` D marks a target, after which `*` means "hint C / clear H0.8"; Settings cover `*` D opens the list, where the cursor row may be Power off (H1.5). So:
- `InputLogic` latches the key's `KeyConfig` at Down. Later `configure()` calls apply to the next press of that key only.
- A press latched as **OnDown** emits `Down`, `Short` (same `tDown`), `Repeat` events only if the latched config has `repeat`, and `Up`. It never emits a Click-`Short` or a `Long`, and never shows a hold fill.
- A press latched as **OnClick** uses the latched `holdMs` for the hold fill, the hesitation zone and `Long`.
- `UiController` likewise captures the key's `Binding` at Down and dispatches that press's later events from the captured binding. If the top-most layer changed in between (alert raised, card shown, slide to another page), the press's remaining events are dropped.

**Stuck key at runtime** (D26). When `InputLogic` marks a key stuck it emits `InputEvent::Stuck`; `AppCore` logs `W KEY  K3 (GPIO6) held 10 s, ignored`, raises `E-KEY-STUCK` and shows the KEY STUCK card (runtime variant, §3.8.8) at once. When the key reads HIGH for 1 s it emits `Unstuck`: log line, health back to Ok, toast "Key # works again". While a key is stuck, gestures of the other keys work normally.

### 2.3 Global behaviours

1. **Wake swallow.** While the display is DIM or OFF, the first Down of any key only restores full brightness (fade-in 120 ms). That key is swallowed until its release, including any hold or repeat. Nothing else happens.
2. **Deep-sleep wake swallow.** At boot, every key that reads LOW is swallowed until released; this includes the key named by `esp_sleep_get_ext1_wakeup_status()` when it is still down. A key that is already up is not swallowed (a brisk tap that woke the gadget has ended; its next press must work).
3. **Screen timeout** (counts from the last key event, including swallowed ones):
   - at *timeout − 5 s* the display goes **DIM** (3 s when the timeout is 10 s);
   - at *timeout* it fades out over 250 ms and turns off (`0xAE`).
   - The timeout is suspended while a non-stuck key is held (§2.2), during Calibration (max 120 s), on Diagnostics › Buttons, on a FATAL card, and while the alert is active (the alert has its own timers).
4. **Timer alert, anywhere.** It applies on any page or modal, while DIM or OFF, from light sleep (timer wake) and from deep sleep (RTC timer wake → boot):
   - The display turns on at High brightness and the **ALERT overlay** appears.
   - Phase A: 30 s of hardware-invert flashing at 2 Hz (`0xA7`/`0xA6`, no redraw).
   - Phase B: steady, up to 5 min.
   - Then the display turns off and a **missed-alert bell chip** stays on the status line until the Timer page is shown.
   - Any key Down dismisses the alert (swallowed until release). At carousel level the UI then slides to the Timer page (Done state). If a modal was open, the UI returns to it; nothing is lost.
   - If a deep-sleep timer wake finds no restorable countdown, the display turns on with the TIMER LOST card instead (§5.4, D32).
5. **Background chips** (R6 made visible). They sit on the top line of every carousel page (Home, Stopwatch, Timer, Settings cover; on Compass, which has no status line, at the top right, §3.8.4). Right to left, in priority order: warning `⚠` (any WARN/FAULT), missed-alert bell, timer running with time left, stopwatch running. A page never shows the chip of its own model (no stopwatch chip on Stopwatch; no timer chip or bell on Timer). Timer chip text (`Micro`, ≤ 3 characters): under 1 min `42S` (seconds, rounded up); under 1 h `5M` (minutes, rounded up); from 1 h `1H` … `10H` (hours, rounded). Space is budgeted by §3.4: on overflow the lowest-priority items are folded or dropped, never overlapped (D30). Modal status lines (list, picker, set time, diagnostics) show no chips.
6. **Navigation memory.**
   - The carousel page is kept in `RtcState` and survives display-off, deep sleep and resets.
   - A modal survives display-off, but an **uncommitted picker or set-time edit is reverted** when the display turns off.
7. **Toasts** confirm changes that are otherwise invisible ("Target 045°", "+1 min, 5:37 left", "Saved", "Time set -1.8 s", "Temp sensor back", "Hold # to reset"). All toast text is ASCII and ≤ 105 px (D29; `°` = `kDegChar`). They last 1.2 s and never take keys. Errors that need action use cards and tags, not toasts. **Every rejected input from the host or hardware gets a toast with the reason** (sync rejected, bad command, settings reset).
   - **Queue**: up to `kToastQueue` = 4 pending toasts, shown one after another; an identical consecutive toast is merged; on overflow the oldest pending one is dropped and counted (log). The queue's timers are **paused** while a card, the splash, the power-off card or the alert is on screen, and while the display is not On, so boot warnings raised under the splash or the CHECK HARDWARE card are still seen afterwards.
8. **USB host present** (debounced, §7.3): no light sleep and no *auto* power-off, so the COM port stays up. Manual power-off still works. While the display is off, a newly plugged host is noticed within about 35 s without a key press (D28, §7.2); a key press makes it immediate.

### 2.4 Bindings per screen state (the single source of truth)

Each screen state returns a `Bindings` struct (§4.4 `ui/bindings.h`). The rail draws from it, the dispatcher executes from it, and `hg_render --bindings` generates `docs/BUTTONS.md`. Notation: **D** = fires on Down, **C** = on Click, **H0.8**/**H1.5** = hold seconds, **R** = repeat. Label column: text shown next to the keycap (Body font): *static* (always) or *pill* (inverted, only while holding).

| Screen / state | `^` | `v` | `#` | `#` icon / label | `*` | `*` icon / label |
|---|---|---|---|---|---|---|
| Home | page− D | page+ D | details on/off D | I_INFO | display off C · **power off H1.5** | I_SLEEP → I_POWER while held / pill "OFF" |
| Home · details | page− | page+ | back to Home D | I_BACK | display off C · power off H1.5 | as above |
| Stopwatch · idle | page− | page+ | — | none | **start** D (`tDown`) | I_PLAY / static "START" |
| Stopwatch · running | page− | page+ | **lap** D (`tDown`) | I_FLAG | **pause** D (`tDown`) | I_PAUSE |
| Stopwatch · paused | page− | page+ | hint toast C · **reset H0.8** | I_RESET / static "HOLD" → pill "HOLD" | **resume** D | I_PLAY |
| Timer · select | page− | page+ | next preset D | I_NEXT | **start** D | I_PLAY |
| Timer · running | page− | page+ | +1 min D | I_PLUS / static "+1 MIN" | **pause** D | I_PAUSE |
| Timer · paused | page− | page+ | hint toast C · **cancel H0.8** | I_X / static "HOLD" → pill | **resume** D | I_PLAY |
| Timer · done | page− | page+ | new → select D | I_X / static "NEW" | restart same preset D | I_RESET / static "AGAIN" |
| Alert overlay (`railHidden`) | dismiss D | dismiss D | dismiss D | (no rail; screen text "Any key = OK") | dismiss D | (no rail) |
| Compass · no target | page− | page+ | calibrate D | I_CAL (static "CAL" when uncalibrated) | **mark target** D | I_TARGET |
| Compass · target set | page− | page+ | calibrate D | I_CAL | hint toast C · **clear H0.8** | I_X → pill "HOLD" |
| Compass · no/unknown sensor | page− | page+ | — | none | re-probe now D | I_RETRY |
| Settings · cover | page− | page+ | — | none | **open list** D | I_ENTER / static "OPEN" |
| Settings · list | cursor↑ D R | cursor↓ D R | back to cover D | I_BACK | value row: edit D · action row: open D · hold row: hint C + **H1.5** · info row: — | I_EDIT · I_ENTER · I_POWER (pill "HOLD") · none |
| Picker | item above D R(accel) | item below D R(accel) | cancel (revert) D | I_X | save D | I_CHECK |
| Set time | field +1 D R(accel) | field −1 D R(accel) | previous field / cancel on first D | I_BACK | next field / save on last D | I_NEXT → I_CHECK |
| Calibration · collecting | — | — | cancel D | I_X | — | none |
| Calibration · ready | — | — | cancel D | I_X | save D | I_CHECK / static "SAVE" |
| Calibration · result | — | — | — (good) / back D (fail) | none / I_BACK | OK D (good) / retry D (fail) | I_CHECK "OK" / I_RETRY "AGAIN" |
| Diagnostics (pages 1,2,4) | page− D | page+ D | back D | I_BACK | page action D | page icon (I_HEAT, I_SCAN, I_RESET) |
| Diagnostics · System | page− | page+ | back D | I_BACK | hint C · **reboot H1.5** | I_POWER → pill "HOLD" |
| Diagnostics · Buttons | captured | captured | captured | rail boxes | captured | rail boxes; **any key held 2 s exits** (also 10 s idle) |
| Card (CHECK HARDWARE, FATAL, KEY STUCK, STATE LOST, settings reset, SHT3x info) | — | — | — | none | OK D | I_CHECK / static "OK"; non-FATAL cards also auto-dismiss after 8 s (so a stuck `*` cannot trap the UI) |

Rules enforced by `test_keymap` (§10.3):
- A `#`/`*` binding with no icon has no short and no long action. `^`/`v` have an action only when the rail shows page dots or chevrons. Hold actions must be advertised by a label (static or pill) or a hold icon.
- **Exemption**: a state with `railHidden = true` (only the alert overlay) is exempt from the two rules above **if** all four keys share the same short action, none has a long action, and the screen draws the string `S_ANY_KEY_OK` ("Any key = OK"), checked through the `DrawRecorder`.
- Every hold binding with a short "hint" action names the hint string.
- No binding's short action on Down coexists with a long action.

### 2.5 State machines

**UI navigation**
```
CAROUSEL(page ∈ HOME, STOPWATCH, TIMER, COMPASS, SETTINGS)
  ^ Down → page−1 (wrap), slide DOWN        v Down → page+1 (wrap), slide UP
  A nav key during a slide jumps the running slide to its end, then starts the next one (no queue).
MODALS (stack depth ≤ 2): HOME.details (flag, not a modal) | SETTINGS.list → {PICKER | SET_TIME | CALIBRATION | DIAGNOSTICS}
                          COMPASS → CALIBRATION
CARDS (above modals): BOOT_REPORT | FAULT card | KEY_STUCK | I2C_STUCK | STATE_LOST | CFG_RESET | SHT3X_INFO   (one at a time, queued in this order)
OVERLAYS (priority): ALERT > POWER_OFF > TOAST (queue of 4, paused under cards/splash/alert/display-off)
```

**Stopwatch** (`core/stopwatch`; all times `MonoUs`)
```
IDLE    ──* D──▶ RUNNING{runStart = tDown}
RUNNING ──* D──▶ PAUSED{elapsed += tDown − runStart}
PAUSED  ──* D──▶ RUNNING{runStart = tDown}
RUNNING ──# D──▶ RUNNING + lap(split = elapsed(tDown))   ring of 99 splits; lapNo keeps counting up to 999; Serial "LAP n split lap"
                 (lapNo == 999 → lap refused, toast "Max 999 laps")
PAUSED  ──# H0.8─▶ IDLE{elapsed = 0, laps cleared}       (# Click → toast "Hold # to reset")
RUNNING ──elapsed ≥ 99:59:59.99──▶ PAUSED + toast "Max time"
```

**Timer** (`core/countdown`)
```
SELECT(i) ──# D──▶ SELECT((i+1) mod 6)
SELECT    ──* D──▶ RUNNING{deadline = tDown + preset; total = preset}     preset index saved to NVS 'tmpre'
RUNNING   ──* D──▶ PAUSED{remaining = deadline − tDown}
PAUSED    ──* D──▶ RUNNING{deadline = tDown + remaining}
RUNNING   ──# D──▶ RUNNING{deadline += 60 s; total += 60 s}  toast "+1 min, 5:37 left"
                   (total + 60 s > 10 h → no change, toast "Max 10 h")
PAUSED    ──# H0.8─▶ SELECT                                              (# Click → toast "Hold # to cancel")
RUNNING   ──now ≥ deadline──▶ DONE{doneAt = deadline} + raise ALERT      tick() returns true exactly once
DONE      ──* D──▶ RUNNING{same preset}         DONE ──# D──▶ SELECT
```
Displayed remaining = `ceil(remaining / 1 s)`. It shows `05:00` at the start, and the alert fires exactly when `00:00` would appear.

**Compass target**
```
NONE ──* D──▶ SET{b = round(true heading)}   toast "Target 045°"    (the same press ends there: no hint, no clear, §2.2)
SET  ──* H0.8─▶ NONE                          toast "Target cleared"     (* Click → toast "Hold * to clear")
any  ──# D──▶ CALIBRATION modal (returns to Compass)
```

**Settings**
```
COVER ──* D──▶ LIST(cursor = last used, RAM only)
LIST: ^/v move the cursor (clamped at the ends, repeat); # → COVER
LIST ──* on Enum/Number row──▶ PICKER{pending = current}; the preview is applied live where meaningful (§8)
PICKER: ^/v change pending; * → validate → NVS write of that key → toast "Saved" → LIST; # → revert → LIST; display-off → revert
        (IKvStore::put() false → the value stays applied for this session, toast "Not stored: storage", E-NVS raised: card once per boot, log every time)
LIST ──* on action row──▶ Set time → SET_TIME | Calibrate → CALIBRATION | Diagnostics → DIAGNOSTICS
LIST ──* H1.5 on Power off / Reset settings──▶ action      (* Click → toast "Hold * to power off" / "Hold * to reset")
     (a press that opened the list or a picker never continues into a hold: press latching, §2.2)
```

**Set time** (fields in order HH → MM → DD → MO → YYYY; seconds become 00)
```
^/v: ±1 in the active field, wrapping inside its range (YYYY clamps 2025..2099; DD clamps to month length after MO/YYYY changes)
*  : next field; on YYYY → commit: TimeService::setLocal(fields, tz, ClockSource::Manual) → toast "Time set" → LIST
#  : previous field; on HH → cancel → LIST
Initial fields: the current local time if the clock is not None, else build date at 12:00.
```

**Calibration** (`core/mag_calibrator`, §6.4)
```
COLLECTING ── coverage ≥ 90 % ∧ grade ≥ OK ∧ ≥ 8 s ──▶ READY (* SAVE appears; collection continues and may improve the grade)
READY      ──* D──▶ RESULT(GOOD|OK) and NVS 'cal' saved
COLLECTING|READY ──# D──▶ back (old calibration kept), toast "Not saved"
COLLECTING ── 120 s ──▶ RESULT(FAILED, reason)          RESULT(FAILED) ──* D──▶ COLLECTING (restart) ; ──# D──▶ back
RESULT(GOOD|OK) ──* D──▶ back to the opener (Compass or Settings list)
```

**Diagnostics**: pages 1..5 with `^`/`v` (wrap), `#` back. The Buttons page (3) captures all keys: it counts presses and lights rail boxes. Holding any key 2 s, or 10 s without input, releases the capture and moves to page 4 (normal bindings again; `#` then goes back).

**Power**: §7.1.

---

## 3. Visual design system (128×64 monochrome SSD1306)

All coordinates below were rendered with the installed U8g2 2.36.19 and inspected at 4× [V-render]. The prototype is scratch code, not the implementation; the production renderer (§10) must reproduce these positions from the real screen code.

### 3.1 Principles

1. **Numbers are heroes.** One Logisoso hero number per page; everything else at least two sizes smaller.
2. **The right edge speaks for the keys.** The rail shows only actions that exist now. No keycap means the key does nothing (§2.4 rules).
3. **One grammar** (§2.2). A hold always shows a fill.
4. **Calm.** Nothing blinks except paused timer digits and the alert. Motion only explains spatial change (≤ 180 ms).
5. **Black is the canvas.** Keep about 25 % or fewer pixels lit in normal views (OLED current, burn-in, night vision). Inversion is reserved for selection, tags, pills, the alert and card headers.
6. **Dotted hairlines** (every 2nd pixel) separate zones.
7. **Honest states.** Approximate, stale, missing and uncalibrated are shown in place, in words.
8. **Legibility floor.** Text a user must read (date, units, messages, hints, keycap labels, error codes) uses `Body` (7 px caps) or larger. `Micro` (5 px) only for the status line, tags, section headers and Diagnostics.
9. **Pixel-exact.** Every coordinate is a `theme::` constant or derived from one. No magic numbers in screen code (layout audit, §10.3).

### 3.2 Grid (`ui/theme.h`)

```
x: 0 ───────────────────────────────────────── 116 │117 118│119 ────── 127
y0   status line: Micro ink rows 1..5               │ gutter │ page dots y 1..~25 (or ▲ y1..3, ▼ y15..17)
y8   ┄┄┄┄┄┄┄┄┄ dotted rule (even x 0..116) ┄┄┄┄┄┄┄  │ empty  │
y10  body                                          │        │
y42                                                │        │ [#] keycap rows 42..51  (physical # ≈ row 48..52)
y54                                                │        │ [*] keycap rows 54..63  (physical * ≈ row 82..87, below glass)
```

| Constant | Value | Notes |
|---|---|---|
| `kW`, `kH` | 128, 64 | |
| `kContentX1` | 116 | content x 0..116 (117 px); x 117..118 always empty |
| `kRailX`, `kRailW` | 119, 9 | rail x 119..127 |
| `kStatusInk`, `kStatusRuleY`, `kBodyTop` | 1, 8, 10 | pages without a status line say so in §3.8 |
| `kDotsX`, `kDotsTop` | 122, 1 | active page = bar 3×6 at x 122..124, then advance 8; inactive = 2×2 at x 123..124, advance 4 |
| `kChevX`, `kChevUpY`, `kChevDnY` | 121, 1, 15 | 5×3 glyphs replace the dots when `^`/`v` move inside a modal |
| `kCapHashY`, `kCapStarY`, `kCapH`, `kCapW` | 42, 54, 10, 9 | keycap `rframe(119, capY, 9, 10, r=1)` |
| `kCapIconX`, `kCapIconDy` | 121, 1 | icon 5×7 at (121, capY+1) |
| `kLabelRight`, `kLabelInkDy` | 116, 2 | label Body, right-aligned, ink rows capY+2..capY+8 |
| `kRailSlotY[4]` | {1, 15, 42, 54} | Diagnostics › Buttons boxes 9×9 next to `^ v # *` |
| `kKeyRowEstimate[4]` | {−19, 16, 48, 80} | documentation only; tuned on hardware [HW?] |

### 3.3 Typography (exact U8g2 names, measured on 2.36.19 [V-render])

**Ink-top convention.** Every `y` in this spec is the **first lit row** of caps and digits. All nine fonts below put ink 1 row below the `u8g2_SetFontPosTop` origin, so `Canvas::text(x, inkTop, …)` draws at `inkTop − 1`. `inkTop` must be ≥ 1; the Canvas rejects and counts anything less. U8g2 coordinates are unsigned 16-bit [HWF u8g2 U27]. A host test re-measures the offset of each font and fails if a U8g2 update changes it.

| Role (`Font::`) | U8g2 font | Ink height (caps/digits) | Digit advance | Tabular? | Used for | Flash |
|---|---|---|---|---|---|---|
| `Hero` | `u8g2_font_logisoso30_tn` | 31 | 19 (':' 11) | yes, by advance | Home HH:MM, Timer MM:SS, alert overtime | 481 B |
| `Big` | `u8g2_font_logisoso28_tn` | 28 | 18 (':' 10) | yes | Stopwatch MM:SS | 427 B |
| `BigS` | `u8g2_font_logisoso24_tn` | 24 | 15 (':' 9) | yes | Stopwatch ≥ 1 h, Timer ≥ 1 h, Timer done, set-time | 421 B |
| `Heading` | `u8g2_font_logisoso22_tn` | 23 | 14 | yes | Compass heading | 387 B |
| `Mid` | `u8g2_font_logisoso16_tr` | 16 | 10 (':' '.' 6) | yes | seconds, fractions, T, RH, numeric picker, calibration % | 1564 B |
| `Label` | `u8g2_font_helvB08_tr` | 8 | 6 | yes | cardinals, timer chips, card titles, steer delta, date in set-time | 1028 B |
| `Value` | `u8g2_font_helvB10_tr` | 11 | ≈8 | n/a | picker current value, splash wordmark | 1333 B |
| `Body` | `u8g2_font_haxrcorp4089_tr` | 7 (descender 2) | 6, but '1' = 3 → **`num()`** | **no** | all readable text: date, units, messages, list rows, laps, labels, hints | 935 B |
| `Micro` | `u8g2_font_tom_thumb_4x6_mr` | 5 | 4 (monospace) | yes | status line, tags, section headers, Diagnostics | 782 B |

Total font flash: **7,358 B** (`nm -S` of the 2.36.19 font objects) [V-render].

Rules:
- **Width functions.** `u8g2_GetStrWidth` adds the *bitmap* width of the last glyph, not its advance. Measured: `"12:34"` = 91 px but `"88:88"` = 87 px in `logisoso30_tn`. Right/centre alignment of **changing numbers** must therefore use `Canvas::numWidth()`: Σ glyph advances (`u8g2_GetGlyphWidth`), with every digit counted as the `'0'` advance. Static text uses `Canvas::width()` (= `u8g2_GetStrWidth`).
- **`Canvas::num()`** draws each digit centred in a cell of `advance('0')`, so `Body` digits (where '1' is 3 px) do not jitter. Use it for every value that changes in place.
- **ASCII only.** The `_tr`/`_tn`/`_mr` fonts contain only 0x20..0x7E, and `DrawStr` silently skips anything else (`·`, `−`, `°` are absent: `u8g2_IsGlyph` = 0 [V-meas]). Every UI string is printable ASCII; `test_strings` and `lint` enforce it (D29). Use `-` for minus and `,`/`:` instead of `·`.
- **Degree sign** is a primitive, because the fonts lack U+00B0:
  - small: 3×3 diamond, pixels (1,0) (0,1) (2,1) (1,2), drawn at (x, inkTop); advance 4 px;
  - large: `circle(x+2, y+2, r=2)` via `Canvas::degree(x, inkTop, true)` (Heading, Home temperature).
  - **`kDegChar`** = the backtick byte (0x60) is reserved: inside any string passed to `Canvas::text()`/`num()` it is drawn as the small degree, and `width()`/`numWidth()` count it as 4 px. In this spec a `°` inside a UI string (`"Target 045°"`, `TGT 120°`, `75°`, `0° up`, `°C`) stands for that byte.
- **Case.** `Micro` and `Label` are ALL CAPS. `Body` is sentence case except abbreviations and units (`MSK`, `SHT4x`, `uT`).
- **Font mode** is transparent (`u8g2_SetFontMode(1)`), so text over boxes works with colour 0. XOR (colour 2) is never used for circles or discs [HWF u8g2 U21].

### 3.4 Chrome components

| Component | Exact spec |
|---|---|
| **Status line** | Title `Micro` at (0,1), optional tag after it (§3.8). Right group right-aligned to x 116: clock `Micro` (`numWidth`, 20 px), then 3 px gap, then chips right-to-left with 3 px gaps in priority order: warning (`S_WARN`, 5 px), bell (`S_BELL5`, 5), timer (`S_TMR` 5×5 + 1 px + `Micro` text ≤ 12 px = 18), stopwatch (`S_SW`, 5). Chip icons at y 1. Dotted rule y 8, even x 0..116. **Budget** (D30): the right group may not extend left of `L = (right end of title or tag) + 3`. Layout pass: place items right-to-left; the first item that would cross `L` and every item after it is dropped (⚠ always fits). Drops are allowed only in the priority order above, never overlaps. Worst case measured [V-meas]: Stopwatch page with `PAUSED`, ⚠ and bell → `L` = 70, the timer chip (x 60..77) is dropped. |
| **Home top line** | Date `Body` (0,1) (widest `MON 28 MAR` = 55 px), zone `Body` R@116 (widest `OMST`/`SAMT` = 25 px). Between them, right-to-left from zone start − 3: ⚠, bell, timer, stopwatch, then the `SYNC?` tag (23 px), 3 px gaps, `L` = date end + 3. Overflow steps, in order: (1) the date drops its weekday (`28 MAR`, 33 px; frees 22 px); (2) the `SYNC?` tag is removed and the ⚠ chip is shown instead (Approx then counts as a warning; Details still says `Approx: build`); (3) chips are dropped from the lowest priority. With the measured widths step 3 never triggers: all four chips = 42 px ≤ 53 px free with `OMST` and no weekday; `SYNC?` + one icon-only chip (⚠, bell or stopwatch) fits even with the weekday [V-meas]. Step 1 comes first because the `SYNC?` word is the honest state (§3.1 rule 7), the weekday is a convenience. |
| **Page dots** | Rail, from y 1: inactive 2×2 at x 123..124 (advance 4), active 3×6 at x 122..124 (advance 8). During a slide the active bar's height morphs 6 → 2 and 2 → 6 along the easing. |
| **Chevrons** | `▲` 5×3 at (121,1), `▼` 5×3 at (121,15). Shown instead of dots whenever `^`/`v` move inside a modal. |
| **Keycap** | `rframe(119, capY, 9, 10, r=1)` + icon 5×7 at (121, capY+1). **Pressed** (Down until Up, min 80 ms): `rbox` filled, icon in colour 0. **Hold** (from 150 ms): fill from the bottom inside x 120..126, height `round(8·p)` with `p = (held−150)/(holdMs−150)`; icon in XOR. At p = 1 the cap flashes filled for 80 ms. No binding → nothing drawn. |
| **Keycap label, static** | `Body`, right-aligned with ink ending at x 116, ink top capY+2. Drawn only in states that list it in §2.4. The layout audit requires the label box `(116−w−1 .. 116, capY .. capY+9)` to be free of other content ink. |
| **Keycap label, pill** (while holding) | Halo `box(116−w−5, capY, w+6, 10)` in colour 0, then `rbox(116−w−4, capY, w+5, 10, r=2)`, then the text in colour 0 right-aligned at x 115, ink capY+2. Transient: it may cover content; the audit exempts it. |
| **Tag** | `rbox(x, y, w+3, 7, r=1)` + `Micro` text in colour 0 at (x+2, y+1). Words: `SYNC?`, `PAUSED`, `DONE`, `UNCAL`, `FIELD?`, `SATURATED`, `NOT FOUND`, `UNKNOWN`. |
| **Toast** | `Body` text, **≤ `kToastMaxTextW` = 105 px** (so that the halo stays inside x 0..116). Width `w = textW + 10`, x = (117 − w)/2. Halo `box(x−1, 49, w+2, 15)` colour 0, `rbox(x, 50, w, 13, r=3)`, text colour 0 at (x+5, 53). Slide-up 100 ms, hold 1200 ms, slide-down 100 ms. One at a time from a queue of 4 (§2.3.7). A wider text is a test failure, not a runtime truncation. |
| **Card header** | `rbox(0, 0, 117, 12, r=2)`, icon `S_WARN` colour 0 at (3,3), `Label` title colour 0 at (11, 2). |
| **Dotted rule** | `dottedH(x, y, w)` = pixels at x, x+2, … |
| **Battery glyph** | `frame(x, y, 12, 7)` + tip `box(x+12, y+2, 2, 3)`. Unknown: three dashes `box(x+2+3i, y+3, 2, 1)`. Known: `box(x+2, y+2, round(8·pct/100), 3)`; outline blinks 1 Hz at ≤ 10 %. |
| **Progress bar** | `frame(0, y, 117, 5)`; fill `box(1, y+1, ⌊115·p⌋, 3)`; quarter ticks: 1 pixel at x 29/58/87, row y+2, colour 0 where filled. |

### 3.5 Icons (`ui/icons.cpp`, ASCII art, `#` = lit, drawn pixel by pixel; 5×7 unless noted)

```
I_PLAY   I_PAUSE  I_FLAG   I_RESET  I_CAL    I_PLUS   I_CHECK  I_X      I_BACK   I_INFO
#....    .....    #....    .###.    .###.    .....    .....    .....    .....    ..#..
##...    ##.##    ####.    #...#    #...#    ..#..    ....#    #...#    ..#..    .....
###..    ##.##    #####    ....#    #...#    ..#..    ...##    .#.#.    .#...    .##..
####.    ##.##    ####.    #..#.    .###.    #####    #.##.    ..#..    #####    ..#..
###..    ##.##    #....    ##...    #...#    ..#..    ###..    .#.#.    .#...    ..#..
##...    ##.##    #....    ###..    #...#    ..#..    .#...    #...#    ..#..    ..#..
#....    .....    #....    .....    .###.    .....    .....    .....    .....    .###.

I_SLEEP  I_POWER  I_TARGET I_NEXT   I_EDIT   I_ENTER  I_RETRY  I_HEAT   I_SCAN
.....    ..#..    .....    .....    ....#    .....    ..##.    ..#..    .....
#####    #.#.#    .###.    .#...    ...##    ..#..    #.#.#    .##..    #.#.#
...#.    #.#.#    #...#    ..#..    ..##.    ...#.    ##...    .###.    .....
..#..    #...#    #.#.#    ...#.    .##..    #####    ###..    ####.    #.#.#
.#...    #...#    #...#    ..#..    ##...    ...#.    .....    ##.##    .....
#####    .###.    .###.    .#...    #....    ..#..    #...#    ##.##    #.#.#
.....    .....    .....    .....    .....    .....    .###.    .###.    .....

5×5 status: S_SW .###. #.#.# #.##. #...# .###.   S_TMR ##### .#.#. ..#.. .#.#. #####
            S_WARN ..#.. .#.#. .#.#. #...# #####  S_FLAME ..#.. .##.. .###. ##.## .###.
            S_OK ....# ...#. #.#.. .#... .....    S_FAIL #...# .#.#. ..#.. .#.#. #...#
            S_BELL5 ..#.. .###. .###. ##### ..#..
7×7 S_BELL  ...#... ..###.. .#####. .#####. .#####. ####### ...#...
3×5 I_LIVE  #.. ##. ### ##. #..            chevrons ▲ ..#.. .###. #####   ▼ ##### .###. ..#..
```
`I_CAL` is a figure-8 (the calibration motion), deliberately distinct from `I_RESET` and `I_X`.

### 3.6 Motion (all time-based: position = f(t), so a slow flush drops frames, never slows animations)

| # | Motion | Spec |
|---|---|---|
| 1 | Page slide | Vertical, `kSlideMs` = 180 ms, ease-out cubic `d = round(64·(1−(1−t)³))`. Only the content area x 0..116 moves; the rail stays, the active dot morphs, keycaps swap at t = 0.5. `ui/compositor` composes the frozen old frame with the live new frame as one `uint64_t` per column (bit = row). `v`: `out = (old >> d) \| (new << (64−d))`. `^`: `out = (old << d) \| (new >> (64−d))`. **`d == 0` and `d == 64` are special-cased**: shifting a 64-bit value by 64 is undefined behaviour. Unit test at d = 0, 1, 7, 8, 63, 64. |
| 2 | Keycap press / hold fill | §3.4. |
| 3 | Toast | §3.4. |
| 4 | Compass card | Critically damped spring on the angle: `θ̈ = ω²·e − 2ω·θ̇`, `e = wrap180(target − θ)`, semi-implicit Euler, `dt` clamped to 50 ms. ω by setting: Calm 6, Normal 12 (≈ 0.4 s settle), Fast 24 rad/s. Shortest path, no 359 → 0 spin. Stays on with Animations = Off (it is filtering, not decoration). |
| 5 | Display wake / sleep | Contrast ramp from 0 to the level in 120 ms (6 steps), fade-out 250 ms, then `0xAE`. Commands only, no redraw. |
| 6 | Alert flash | `0xA7`/`0xA6` every 500 ms during phase A. Zero redraw cost. |
| 7 | Paused timer digits | 1 Hz, 700 ms on / 300 ms off. The stopwatch does not blink when paused; it shows the `PAUSED` tag. |

Setting **Animations = Off** sets motions 1, 3 and 5 to 0 ms. Frame rate is bounded by the flush (§4.5): about 30 fps for full-screen motion.

### 3.7 Brightness, contrast, burn-in

| Level | `0x81` contrast | `0xD9` precharge | `0xDB` VCOMH | Use |
|---|---|---|---|---|
| High | 0xCF | 0xF1 | 0x40 | U8g2 noname init defaults [HWF u8g2 U11]; alert phase A |
| Mid (default) | 0x40 | 0xF1 | 0x40 | indoor and night |
| Low | 0x01 | 0x22 | 0x20 | DIM state and dark adaptation. How much this dims varies by panel [HW?] (§11 step 17) |

Burn-in: only with Screen off = Never, the whole frame shifts by a ±1 px orbit (4 positions) every 60 s.

### 3.8 Per-screen pixel layouts

Notation: `(x, inkTop)`. **R@116** = right-aligned with ink ending at x 116. **C@x** = centred on x. **num** = drawn with `Canvas::num()`. File names refer to `/mnt/wd/arduino-esp32/tmp/spec_final/proto/out/*.png` [V-render].

#### 3.8.1 HOME (page 0, no status rule) — `01_home` … `08_home_hold_off`

| Element | Font / primitive | Position |
|---|---|---|
| Weekday + date `SUN 11 OCT` | Body | (0,1), rows 1..7. Weekday is **computed** from the date (2026-10-11 is a Sunday). Drops the weekday first on overflow (§3.4 Home top line). |
| Zone `MSK` / `CET` / `CEST` | Body | R@116, ink 1 |
| Chips | 5×5 icons at y 2, `Micro` text at ink 2 | right-to-left from the zone in priority order ⚠, bell, timer, stopwatch (§3.4) |
| `SYNC?` tag | tag | left of the chips, 3 px gaps; box y 1..7; folded into ⚠ on overflow (§3.4) |
| Clock `14:05` | Hero, num | (0,11) → rows 11..41, x 0..86 |
| Seconds `27` | Mid, num | (96,11) → rows 11..26, x 96..115 |
| Battery | glyph | (98,33), 14×7 |
| Rule | dotted | y 45, x 0..116 |
| Temperature `23.4` | Mid, num | (0,48) → rows 48..63, width `w` |
| Degree + unit | small degree (w+1, 48); Body `C`/`F` at (w+5, 49) | |
| Heater glyph | `S_FLAME` at (w+5, 58) | while heating or cooling |
| RH `56` + `%RH` | Mid num at x = max(50, w+15), ink 48; Body `%RH` at (x+rw+2, 49) | |
| Dew point | Body `DEW` R@116 ink 48; Body num value R@113 ink 57; small degree (114,57) | **only if the RH group ends ≤ x 93** (e.g. hidden for `-12.3` / `100`; the value stays in Details) |
| Rail | page dots (p0); `#` I_INFO; `*` I_SLEEP (I_POWER + pill "OFF" while held) | |

| State | What changes |
|---|---|
| Clock Approx | `SYNC?` tag (or ⚠ when the line is full); one toast after boot "Clock approx, sync USB" (97 px). |
| Clock None | Clock shows `--:--`; seconds hidden; the date line reads `Clock not set` (Body, 55 px). |
| Sensor missing | Climate row → `S_WARN` at (0,49) + Body `No temp sensor` (8,48) + `Check SDA8 SCL7 3V3` (8,57). |
| Unknown chip | `Unknown chip at 0x44` / `Not SHT4x / SHT3x`. |
| Stale (no valid reading for 60 s while the display is on) | `--.-` and `--`. |
| Heater cooling | Last pre-pulse values frozen + flame glyph. |
| Details (`#`) | Status line `DETAILS` (0,1) + `HH:MM:SS` Micro R@116; rule y 8. Six rows i = 0..5 at **ink 10+9i** (10, 19, …, 55; the last descender ends on row 63): key Body (0, ·), value Body R@116, in this order: Zone `MSK  UTC+3`; Clock `Synced 2h ago` / `Approx: build` / `Approx: restored` / `Manual 3d ago` / `Not set`; Sensor `SHT4x OK` / `SHT3x OK` / `No reply 0x44`; Heater `Off` / `Every 10m, next 4m` / `Cooling 35s` / `Skipped: >60°C` / `N/A (SHT3x)`; Dew point `14.1°C`; Abs. hum. `11.7 g/m3`. An active compass fault replaces the Abs. hum. row (`Compass: not found`). **Battery is not listed** (one rule, also in §6.5: Home shows the glyph; the boot report and `INFO` say `bat not fitted` or the voltage). `#` = I_BACK. The prototype render of this card is outdated (row order, battery row); this row wins. |

#### 3.8.2 STOPWATCH (page 1, status line) — `10_sw_idle` … `15_sw_10h`

| Element | Font | Position |
|---|---|---|
| Title / right group | Micro | `STOPWATCH` (0,1); clock R@116; chips (no stopwatch chip on this page), budget §3.4 |
| `PAUSED` tag | tag | (40,0) → x 40..66; chips must stay right of x 69 (the timer chip is dropped first when ⚠ and bell are also present; fixture `sw_paused_all_chips`) |
| < 1 h | Big num `MM:SS` (0,12) → rows 12..39, width 82; Mid num `.cc` at (w+1, 24) → rows 24..39 | |
| ≥ 1 h | BigS num `H:MM:SS` (0,14) → rows 14..37, width 93; Mid num `.c` at (w+1, 22) | |
| ≥ 10 h | BigS num `HH:MM:SS` (0,14), width 108; no fraction | |
| Rule | dotted y 43, x 0..116 | |
| Lap row A (current lap, live) | Body num `L5` (0,47) in a 3-digit cell (`L999` = 24 px, x 0..23); `I_LIVE` at (24,48); Body num time (29,47) | rows 47..53 |
| Lap row B (last completed) | Body num `L4` (0,56); Body num time (29,56); Body num delta **R@116** ink 56 `-0.42` / `+1:02` | rows 56..62 |
| Idle | `00:00.00`; Body `Ready` (0,47); `*` label `START` | |
| Lap time format | `MM:SS.cc` < 1 h; `H:MM:SS` < 10 h; `HH:MM:SS` ≥ 10 h (≤ 40 px → time ends ≤ x 68). Delta vs the previous lap: `+S.cc` < 60 s, `+M:SS` < 1 h, `+H:MM:SS` ≥ 1 h (≤ 46 px → starts ≥ x 71), sign `-` or `+` [V-meas] | |
| Lap number | `lapNo` counts up to 999; the 1000th lap is refused with toast "Max 999 laps" | |
| Rail | dots p1; `#`: running I_FLAG, paused I_RESET + label `HOLD`, idle none; `*`: idle I_PLAY + `START`, running I_PAUSE, paused I_PLAY | |

#### 3.8.3 TIMER (page 2, status line) — `20_tm_select` … `26_alert_b`

| State | Elements |
|---|---|
| Title / right group | `TIMER` (0,1); clock R@116; chips (no timer chip and no bell on this page), budget §3.4 |
| **Select** | Hero num `05:00` (0,11) → rows 11..41, ink x 2..84. **Right column R@116** (Body; widest line 29 px → starts ≥ x 88, clear of the Hero box ending at x 86): `MIN` ink 12; `ENDS` ink 23; num `14:10` ink 32 (`ENDS` lines hidden when the clock is None). **Chips**: cell i at x 19i, w 17. Label `Label` C@(19i+9), ink 52. Selected: `rbox(19i, 50, 17, 12, r=2)` with the label in colour 0. Rail: `#` I_NEXT, `*` I_PLAY. |
| **Running** | Hero num `04:37` (0,11). Right column R@116: total ink 12 (`5 MIN`; total < 100 min → `N MIN`, ≥ 100 min → `1H 40` … `10H 00`, all ≤ 29 px [V-meas]), `ENDS` ink 23, num `14:10` ink 32. Progress bar at y 56 (rows 56..60), `p = remaining/total` (drains). Rail: `#` I_PLUS + static `+1 MIN`; `*` I_PAUSE. |
| **Running ≥ 60 min** | BigS num `1:00:00` (0,14); right column hidden; Body `60 min  ends 15:05` at (0,45); bar y 56. |
| **Paused** | As Running + tag `PAUSED` at (24,0); digits blink (§3.6). Rail: `#` I_X + static `HOLD` (pill while held); `*` I_PLAY. |
| **Done** | Tag `DONE` (24,0); Body `5 min timer, ended 14:10` (0,12) (total formatted as in Running, ≤ 111 px); overtime **< 1 h**: BigS num `+02:13` (0,22) → rows 22..45, ink x 3..82; **≥ 1 h**: Mid num `+1:02:13` (0,26) → rows 26..41, ink ≤ x 80 (`+99:59:59`; the display stops at 99:59:59). Either way it stays clear of the `NEW` label box (x 96..116, rows 42..51) [V-meas]. Body `Overtime` (0,50). Rail: `#` I_X + `NEW`; `*` I_RESET + `AGAIN`. |
| **Alert overlay** (full bleed, no rail, no status) | `S_BELL` (3,3); Label `TIME UP` (13,3); Body `5 min` R@124 ink 3; Hero num overtime `+0:12` C@64 ink 16 → rows 16..46; Body `Any key = OK` C@64 ink 54. Phase A: hardware invert at 2 Hz; phase B: steady. |

#### 3.8.4 COMPASS (page 3, no status line) — `30_cmp_ok` … `35_cmp_unknown`

| Element | Spec |
|---|---|
| Lubber line | filled triangle (28,0) (34,0) (31,3), fixed |
| Ring | `circle(31,34,29)` → x 2..60, y 5..63 |
| Ticks | rotate with the card (angle = `a − cardDeg`). Every 15° except multiples of 90°: 45° multiples r 28 → 25, others r 28 → 27. Lines drawn with `Canvas::line` (signed, clipped; never `u8g2_DrawLine` [HWF u8g2 U27]). |
| Cardinals | `Label` at r 21. N: `disc` r 5 with a colour-0 `N` at (x−3, y−3). E/S/W: centred at (x − w/2, y − 3), upright. |
| Needle | always points to the card's N. North: filled triangle, tip r 15, base ±3 px at the centre. South: outline, tip r 11. Hub: colour-0 disc r 2 + circle r 2. |
| Target marker | colour-0 disc r 3 + white diamond r 3 at **r 25** (inside the ring) at `target − cardDeg`; dotted radial r 6..19 step 3. At r 25 the disc spans y 6..62 and x 3..59 for every angle, so `Canvas::disc` never refuses it (at r 29 it left the screen for relative bearings 154°–206° and 255°–285°) [V-meas]. Fixtures `cmp_target_rel180`, `cmp_target_rel270` |
| Chips | Compass has no status line: chips (⚠, bell, timer, stopwatch) right-aligned at x 116, icons at y 1, budget `L` = 66 (§3.4); the heading ink starts at row 11 |
| Heading `045` | Heading num (66,11) → rows 11..33, width 42; large degree at (66+w+1, 11) |
| Cardinal name `NE` (16-point) | Label (67,37) → rows 37..44 |
| Line A (ink 48) | Body `True +12°` / `Mag`; or `TGT 120°` (≤ 38 px); or a tag `UNCAL` / `FIELD?` / `SATURATED` at (66,48) |
| Line B (ink 57) | Body `Hold level`; target: Label num steer `75°` + filled triangle pointing to the turn side (right: after the number; left: before it); `\|Δ\| ≤ 3°` → Body `On course`; `FIELD?` → `Metal near?` |
| Rail | dots p3; `#` I_CAL (+ static `CAL` when uncalibrated); `*` I_TARGET (no target) or I_X (target set, hold) |
| **No sensor** | Ghost ring (1 px every 10° on r 29); Mid `?` C@31 ink 26; no title (the ghost ring and `?` say it; the right column ends ≤ x 116); tag `NOT FOUND` (66,12); Body (66,22) `No reply at`, (66,31) `0D 2C 1E`, (66,40) `Check wires`, (66,49) `SDA8 SCL7` (widest 50 px → ends x 115) [V-meas]. Rail: `*` I_RETRY only. |
| **Unknown chip** | Same frame; tag `UNKNOWN` (66,12); Body (66,22) `At 0x0D` (or `0x1E`), (66,31) `ID 0x12`, (66,40) `See Diag.` (all ≤ 37 px). |

#### 3.8.5 SETTINGS (page 4) — `40_set_cover` … `48_set_time`

| State | Spec |
|---|---|
| **Cover** | Status `SETTINGS` + clock; rule y 8. Four Body rows at ink 12/22/32/42: label (0,y), value R@116 (Time zone, Screen off, Heater, Declination). Body `19 items` (0,56). Rail: dots p4; `*` I_ENTER + static `OPEN`. |
| **List** | Status Micro `SETTINGS` + `4/19` R@116; rule y 8. From y 10, rows stacked until y 64: **section header** 9 px tall (Micro caps at (1, y+2); dotted rule from textW+4 to x 112 at y+4); **item** 11 px tall (selected: `rbox(0, y, 114, 11, r=2)` with colour-0 text; label Body (3, y+2); value Body R@111; action rows I_NEXT at (106, y+2); info rows value only). **List values are short forms** (`fmt` `ValueStyle::List`; widest `270° down` ≈ 46 px, so any value starts at x ≥ 65; the longest label `Auto power off` ends at x 63 and its values are ≤ 28 px): `MSK`, `30 s`, `Mid`, `On`, `60 min`, `°C`, `+0.3°C`, `Auto`, `110 mW`, `+12°`, `270° down`, `Normal`, `Good 94%`, `2.0.0`; on SHT3x the Heater and Heater power rows read `N/A` [V-meas]. The audit checks label end + 3 ≤ value start on every row in every option (sweeps, §10.2). Only whole rows are drawn. **Scrollbar**: dotted track x 116, y 10..63; thumb `box(115, y, 2, h)`, `h = max(6, 54·visible/total)`. **Scroll policy**: keep the cursor row fully visible; when moving down, keep one row of look-ahead if available. Rail: chevrons; `#` I_BACK; `*` I_EDIT / I_ENTER / I_POWER (hold). |
| **Picker** | Status: setting name Micro (0,1), `i/19` R@116; rule y 8. Item above Body C@58 ink 11. Box `rframe(0, 19, 116, 30, r=3)` → rows 19..48. Current value `Value` C@58 ink 23 (numbers: Mid num C@58 ink 22). Sub-label / live preview Body C@58 ink 39. Item below Body C@58 ink 52. **Width rule (generic, every picker)**: a `Value` text wider than 110 px is drawn in `Label` instead (e.g. `YEKATERINBURG` 124 → 89 px); sub-label and neighbour items must be ≤ 110 px; anything still wider fails the audit [V-meas]. **Order rule**: `^` always selects the item shown above, `v` the item shown below. Numeric pickers show the larger value above (so `^` = increase); enum pickers show the previous option above (list order). Rail: chevrons; `#` I_X; `*` I_CHECK. Picker values (`ValueStyle::Picker`): TZ title `MOSCOW` (§5.5); mount `270° DOWN` (79 px); heater `OFF` / `AUTO` / `EVERY 5 MIN` … `EVERY 60 MIN` (101 px); heater power `Low 20 mW` / `Mid 110 mW` / `High 200 mW` (95 px). Sub-label examples: TZ `UTC+3 Minsk, Istanbul` (offset now + note, §5.5); TZ neighbours `Kaliningrad UTC+2`; mount `Live heading 045°`; heater `1 s pulse, then cool` / `Pulse when RH >= 90%`; temp offset `Now 23.1°C`; declination `Moscow +12°, see NOAA`; Heater on SHT3x: value `N/A`, sub-label `SHT3x has no pulse heater` (108 px), `*` disabled (no icon). |
| **Set time** | Status Micro `SET TIME  MSK` + `2/5`. BigS num `HH` at x 22 ink 13, `:` then `MM`. Active field `rbox(x−2, 11, w+4, 28, r=2)`, digits colour 0. Date Label C@58 ink 45 `SUN 11.10.2026`; an active date field is inverted (`rbox(x−1, 43, w+2, 11, r=1)`). Rail: chevrons; `#` I_BACK; `*` I_NEXT (I_CHECK on YYYY). |

#### 3.8.6 CALIBRATION (modal, no status line) — `50_cal_progress` … `53_cal_fail`

- Ring centre (31,33), r 27, 24 segments of 15°: covered = radial lines r 27 → 23 every 1.5° inside [15i+1°, 15i+14°]; uncovered = 1 pixel at r 25, angle 15i+7.5°. Live direction marker: disc r 1 at r 19.
- Mid percentage C@31 ink 26.
- Right column Body x 64: `CALIBRATE` ink 2; `Keep level` 14; `Turn slowly` 23; `2 full turns` 32; live `Field 16 uT` 41; hint ink 50: `Turn more` / `Keep level!` / `Turn slower` / `Metal near?` / `Field weak` / `Ready` (widest `Metal near?` 50 px → ends x 113, clear of the `#` keycap at x 119; same wording as Compass) [V-meas]. In READY the hint always reads `Ready`, so it never meets the static `SAVE` label box (x ≥ 95, rows 54..63).
- Rail: `#` I_X; `*` I_CHECK + static `SAVE` only in READY.
- **Result (good/ok)**: disc r 9 at (16,20) with a 3-px colour-0 check; Label `CALIBRATED` (32,12); Body `Quality: good 94%` (32,24); dotted rule y 35; Body rows ink 38/47/56: `Offset  X -312  Y +87`, `Scale   X 1.03  Y 0.97`, `Field 16 uT`. `*` I_CHECK + `OK`.
- **Result (failed)**: circle r 9 + Label `!`; Label `NOT SAVED` (32,12); Body reason (32,24) e.g. `Covered 9 of 24`; rows 38/47/56: `Turn 2 full circles,` / `slowly, kept level.` / `Old calibration kept`. `#` I_BACK, `*` I_RETRY + `AGAIN`.

#### 3.8.7 DIAGNOSTICS (modal; Micro allowed) — `60_diag_sensors` … `64_diag_sys`

Header Micro `DIAG  <PAGE>` (0,1), `p/5` R@116, rule y 8. Rail: chevrons, `#` I_BACK, `*` page action.

| Page | Content |
|---|---|
| 1 SENSORS | Rows Micro, key (0, 12+9i), value (24, ·): `OLED SSD1306 3C OK` · `TEMP SHT4X 44 0F3A21C4` · `MAG QMC5883L 0D ID FF` · `DRDY GPIO12 50/S RISE` (or `NO PIN (QMC5883P)`) · `I2C ERR 0/0 REC 0` · `HEAT 23.4>54.1 D+30.7` (last heater test). `*` I_HEAT = heater test (SHT4x only). |
| 2 I2C SCAN | `BUS0 SDA8 SCL7 100K` / Body address list `0D 44` + Micro names / `BUS1 SDA10 SCL11 400K` / `3C OLED` / `SCANNED 0X08-0X77`. `*` I_SCAN = rescan. |
| 3 BUTTONS | Header shortened to x ≤ 104. Rows Micro (0, 12+10i): `K1 GPIO4  ^ UP     3` (press counter). Body `Hold any key 2 s = exit` (0,54). **Rail boxes 9×9 at `kRailSlotY` {1,15,42,54}**: `rbox` while the matching *logical* key is down, `rframe` otherwise. Shows mapping and rail alignment at once. |
| 4 MAG RAW | `X/Y/Z` Body num raw at (10, 12+10i) + Micro min/max (56, ·); Micro `\|BH\| 16.6UT  RATE 50/S  OVF 0`; `HDG RAW 045.2  CAL 044.7`. `*` I_RESET = reset min/max. |
| 5 SYSTEM | Micro rows at ink 11+7i: `FW 2.0.0 OCT 11 2026`, `CORE 3.3.12 IDF 5.5.5`, `RESET POWERON`, `WAKE EXT1 K2 +212MS` (wake key and `WakeInfo.sampleUs`, the `esp_timer` time at the boot key sample; ROM/bootloader time is not included, so the true wake-to-sample latency is larger; tunes §7.5), `UPTIME 0D 02:14:09`, `USB HOST ON  CDC ON`, `LOOP MAX 6.1MS FLUSH 3.4MS`. `*` I_POWER: hold 1.5 s = reboot. |

#### 3.8.8 Cards and system screens — `70_boot_report` … `75_splash`

| Screen | Spec |
|---|---|
| **CHECK HARDWARE** (boot report, only when a subsystem failed) | Card header with Label `CHECK HARDWARE`. Subsystems, in this order: **I2C bus, Temp, Compass, Storage** (the display is never listed: if this card is visible, it works). Exactly 4 line slots at y = 14, 23, 32, 41 (Body, descender ≤ row 49), footer Body error code (0,56) of the first failure, e.g. `E-SHT-NONE`. Fill: (1) each failed subsystem as `S_FAIL` at (0, y+1) + Body `Temp: no reply 0x44` at (8, y); (2) **one** hint line in total, at (8, y+9) under the first failure (`Check SDA8 SCL7 3V3`); (3) OK subsystems as `S_OK` + `Compass OK` while slots remain (OK rows that do not fit are omitted); (4) if failures remain unshown, the last slot reads `+N more, see Serial`. All row texts ≤ 108 px [V-meas]. `*` I_CHECK + `OK`. Auto-dismiss after 8 s; the owning screens keep showing the state. |
| **I2C BUS STUCK** (FATAL) | Header `I2C BUS STUCK`; Body (0,15) `SDA8 is held low.` (0,24) `Sensors are offline.` (0,33) `Unplug sensor wires,` (0,42) `check for shorts.`; code (0,56) `E-I2C-STUCK`; `*` OK. Clock, stopwatch and timer keep running. |
| **KEY STUCK** | Header `KEY # STUCK` (logical symbol of the key). Body rows at (0,15/24/33/42), code `E-KEY-STUCK` (0,56). Three variants: **runtime** (D26) `K3 (GPIO6) held 10 s,` / `now ignored. Release` / `it or check for a` / `short to GND.`; **at sleep** `K3 (GPIO6) read pressed` / `when going to sleep.` / `It cannot wake the` / `gadget until fixed.`; **phantom wakes** `K3 (GPIO6) woke the` / `gadget 5 times with` / `no press. Excluded` / `from wake until fixed.` (all ≤ 102 px). Auto-dismiss after 8 s (the stuck key may be `*`). |
| **STATE LOST** (`E-RTC-LOST`, D32) | Header `STATE LOST`, or `TIMER LOST` when the wake cause was the timer. Body (0,15/24/33/42): `Sleep memory was lost.` / `Timer, stopwatch and` / `target were reset.` / `Check your timer.`; crash-loop variant: `Restarted 3 times;` / `state was not restored.`; code (0,56) `E-RTC-LOST` (or `W-RST-CRASH`). The display is turned on for it. `*` OK; auto-dismiss after 8 s. |
| **Settings reset** | Header `SETTINGS RESET`; Body names the first reset key, e.g. `'Screen off' was invalid,` / `reset to 30 s.`; code `W-CFG-RESET`. |
| **Power off** | `circle(64,22,10)`, colour-0 gap `box(61,10,7,8)`, bar `box(63,8,3,12)`; Label `POWERING OFF` C@64 ink 38; Body `Press any key to wake` C@64 ink 49; optional Body C@64 ink 57: `Timer will wake me` or `Stopwatch keeps running`. Shown 1 s, then deep sleep. |
| **Splash** (cold boot only, ≥ 600 ms, ≤ 1.5 s while probes run) | Polyline (0,44)(14,34)(22,38)(38,18)(48,28)(58,22)(76,40)(88,30)(100,36)(116,26)(127,32); disc r 1 at (38,18); Value `HIKING GADGET` C@64 ink 47; Micro `V2.0.0` C@64 ink 59. Static (no animation). |
| **OLED missing** | Cannot be shown. Serial prints `E OLED E-OLED-NONE no ACK at 0x3C/0x3D on SDA10/SCL11` every 10 s; the device runs headless (models, timer, Serial). The 10 s retry is only a zero-length probe unless the panel ACKs (§6.6). |
| **Display off** | `0xAE`. |

---

## 4. Architecture

### 4.1 File tree

The project root **is** the sketch folder: `Hiking_Gadget/Hiking_Gadget.ino`. Arduino compiles only the root and `src/` recursively. `tools/`, `docs/` and `legacy/` are copied into the build folder but not compiled (B verified this with `#error` probes). Each file has one owner work package (§13).

```
Hiking_Gadget/
├── Hiking_Gadget.ino                  setup()/loop() only (ctags is stubbed: no other functions)
├── README.md                          Russian: wiring, IDE settings, flashing, sync, error codes, checklist; LINKS the generated key tables (never copies them)
├── docs/  BRIEF.md · HW_FACTS.md · SPEC.md · BUTTONS.md + BUTTONS_RU.md (generated) · HW_CHECKLIST.md · screens/*.png (generated; written only by `make docs`)
├── src/
│   ├── config/                        constants only, no code
│   │   ├── board_pins.h               pins, I2C addresses, key map
│   │   ├── build_config.h             feature flags, bus clocks, build zone
│   │   ├── timing.h                   every duration in ms/µs
│   │   └── version.h                  kFwVersion "2.0.0"
│   ├── core/                          [HOST] pure C++17; no Arduino/IDF/u8g2 headers
│   │   ├── types.h                    MonoUs/RtcUs/UtcMs, Key, Health, Severity, ErrCode, Brightness, ResetReason, WakeCause, BatteryState
│   │   ├── crc.h/.cpp                 crc8Sensirion, crc32, fnv1a32
│   │   ├── civil_time.h/.cpp          civil<->days, weekday, parsers (build stamp, ISO, unix)
│   │   ├── tz.h/.cpp                  18-zone table, EU/US DST, utc<->local
│   │   ├── clock_policy.h/.cpp        ClockQuality/Source/Meta, decideBootClock, validateSync
│   │   ├── stopwatch.h/.cpp           Stopwatch + Snapshot
│   │   ├── countdown.h/.cpp           Countdown + Snapshot
│   │   ├── rtc_bridge.h/.cpp          RtcState layout, seal/verify, restore-time arithmetic
│   │   ├── input.h/.cpp               InputLogic (debounce, click/hold/repeat, swallow, capture-all)
│   │   ├── env_math.h/.cpp            SHT conversions, RH correction, dew point, AH, °F
│   │   ├── heater_policy.h/.cpp       HeaterMode, heaterDue()
│   │   ├── compass_math.h/.cpp        mount, heading, VectorEma, Spring, point16, wrap
│   │   ├── mag_calibrator.h/.cpp      level-only 2-D hard/soft-iron calibration
│   │   ├── settings.h/.cpp            schema table (19 rows), Settings, validate/step/format
│   │   ├── power_policy.h/.cpp        decidePower(), UsbDebouncer
│   │   ├── health.h/.cpp              HealthRegistry, ErrInfo table
│   │   ├── cli.h/.cpp                 parseLine()
│   │   └── log_ring.h/.cpp            fixed ring of log lines
│   ├── drivers/                       [HOST] depend only on II2cBus + core
│   │   ├── i2c_bus.h/.cpp             II2cBus interface, I2cStatus, register helpers (WP0 writes the .cpp in full)
│   │   ├── sht.h/.cpp                 SHT4x/SHT3x probe, measure, heater (non-blocking)
│   │   ├── mag.h/.cpp                 MagDriver facade (probe, start, standby, poll)
│   │   └── mag_chips.h/.cpp           QMC5883L / QMC5883P / HMC5883L register tables and decoders
│   ├── ui/                            [HOST] u8g2 C API only
│   │   ├── u8g2_inc.h                 #ifdef ARDUINO <U8g2lib.h> #else "u8g2.h"
│   │   ├── theme.h                    grid constants (§3.2), Font roles → u8g2 fonts
│   │   ├── canvas.h/.cpp              Canvas over u8g2_t*
│   │   ├── icons.h/.cpp               IconId + ASCII-art bitmaps (§3.5)
│   │   ├── strings.h/.cpp             StrId enum + table built from the strings_*.h X-macro fragments; errTitle/errHint declared (§4.10)
│   │   ├── strings_core.h · strings_screens_a.h · strings_screens_b.h   (created empty by WP0, then one owner each)
│   │   ├── err_text.cpp               errTitle()/errHint() definitions (WP7)
│   │   ├── fmt.h/.cpp                 time/number/setting-value formatting for the UI
│   │   ├── view_types.h               ScreenId, FormatCtx (WP0; breaks the screen.h ↔ view_model.h cycle)
│   │   ├── view_model.h               ViewModel, UiState
│   │   ├── oled_setup.h/.cpp          u8g2 setup + I2C address + init order for the SSD1306/SH1106 (host-tested, used by hal_oled)
│   │   ├── bindings.h/.cpp            ActionId, Binding, Bindings, trigger derivation
│   │   ├── widgets.h/.cpp             status line, rail, keycaps, labels, tag, toast, battery, bar, card header
│   │   ├── compositor.h/.cpp          slide composition (uint64 columns)
│   │   ├── frame_diff.h/.cpp          shadow buffer → dirty page spans
│   │   ├── display_port.h             IDisplay interface (re-exported by app/ports.h)
│   │   ├── screen.h                   Screen interface (forward-declares ViewModel, UiState)
│   │   ├── screens/                   home, stopwatch, timer, alert, compass, settings_cover, settings_list,
│   │   │                              picker, set_time, calibration, diagnostics, cards, power_off, splash (.h/.cpp each)
│   │   │   ├── registry.h/.cpp        const Screen& screenFor(ScreenId) (WP0): asks screenForA(), then screenForB()
│   │   │   └── registry_a.cpp · registry_b.cpp   screenForA (WP8 screens) / screenForB (WP9 screens); WP0 stubs return nullptr
│   │   └── ui_controller.h/.cpp       navigation, modal stack, overlays, toasts, dispatch, frame pacing, flush
│   ├── app/                           [HOST] pure composition over ports
│   │   ├── ports.h                    hardware ports (implemented by hal/ and tools/host/fakes); includes ui/display_port.h
│   │   ├── app_core.h/.cpp            AppCore: boot(), step() → Directive; owns everything below
│   │   ├── time_service.h/.cpp        wall clock + quality + sync + checkpoint
│   │   ├── thermo_service.h/.cpp      SHT scheduling, heater, cooldown, publishing
│   │   ├── compass_service.h/.cpp     magnetometer demand, filters, spring, calibration session
│   │   ├── power_manager.h/.cpp       display states, sleep directives, shutdown sequencer
│   │   ├── persist.h/.cpp             NVS keys (settings, cal, clk, bld, tmpre, boots) over IKvStore
│   │   ├── console.h/.cpp             executes parsed commands, replies, toasts
│   │   └── logger.h/.cpp              log API → LogRing + paced output to ISerialPort (§7.3)
│   └── hal/                           [ESP32 ONLY] Arduino/IDF; never compiled on the host
│       ├── build_checks.h             first include of every hal/*.cpp: #error for core < 3 and for USB CDC On Boot / USB Mode
│       ├── board.h/.cpp               composition root: boardSetup(), boardLoop()
│       ├── hal_i2c.h/.cpp             II2cBus over TwoWire (stuck check, recovery, scan)
│       ├── hal_oled.h/.cpp            IDisplay: u8g2 setup + Wire1 byte callback
│       ├── hal_keys.h/.cpp            IKeys (GPIO read, wake-pin release)
│       ├── hal_clock.h/.cpp           IMonoClock, IRtcClock, IWallClock (only file allowed to call settimeofday)
│       ├── hal_power.h/.cpp           IPower: light/deep sleep, wake info, USB raw, restart
│       ├── hal_store.h/.cpp           IKvStore (Preferences) + IRtcStore (RTC_NOINIT)
│       ├── hal_serial.h/.cpp          ISerialPort over HWCDC
│       ├── hal_battery.h/.cpp         IBattery (stub unless kPinBatteryAdc ≥ 0)
│       └── hal_drdy.h/.cpp            IDrdy edge counter (Diagnostics only)
└── tools/                             never compiled by Arduino
    ├── build.sh                       fw | host | test | render | lint | probe | all
    ├── lint.sh                        grep rules §10.4
    ├── sync_time.py                   PC → device time (pyserial)
    ├── sync/hg_sync.html              same, Web Serial (Chrome/Edge, no Python)
    ├── probe/api_probe/api_probe.ino  HAL API compile probe (copy of the spec's probe)
    └── host/
        ├── Makefile                   libu8g2.a from the installed U8g2; hg_tests; hg_render
        ├── tiny_test.h                ~60-line TEST/CHECK/CHECK_EQ/CHECK_NEAR
        ├── fakes/                     FakeClock, FakeKeys, FakeDisplay, FakeKv, FakeRtcStore, FakePower, FakeSerial, FakeI2c (+ device models)
        ├── fixtures.h/.cpp            fixture registry + helpers; fixtures_a.cpp (WP8) / fixtures_b.cpp (WP9) /
        │                              fixtures_nav.cpp (WP10: slides, toasts) register the scenarios and sweeps (§10.2)
        ├── render_main.cpp            scenarios → PBM
        ├── bindings_doc.h/.cpp        Bindings of every fixture → BUTTONS.md / BUTTONS_RU.md (hg_render --bindings [--lang ru])
        ├── pbm2png.py                 4× PNGs + contact sheet (PIL)
        ├── golden/*.pbm               accepted renders
        └── tests/test_*.cpp
```

### 4.2 Layers and rules

```
Hiking_Gadget.ino ─▶ hal/board ─▶ implements app/ports.h ◀── app ──▶ core
                                                           app ──▶ drivers ──▶ core
                                                           app ──▶ ui ──▶ core (read-only via ViewModel), u8g2 C API
```

1. `core/`, `drivers/`, `ui/` and `app/` must not include `Arduino.h`, `Wire.h`, `esp_*.h`, `driver/*`, `freertos/*`, and must not call `millis()`, `delay()`, `gettimeofday()` or `settimeofday()`. `ui/u8g2_inc.h` is the only file there that branches on `ARDUINO`. `tools/build.sh lint` enforces this with grep, together with:
   - no `U8G2_.*2ND_HW_I2C` anywhere (it compiles to a silent no-op on arduino-esp32: `U8X8_HAVE_2ND_HW_I2C` requires `WIRE_INTERFACES_COUNT > 1`, which core 3.3.12 never defines, [HWF u8g2 U2/U3]);
   - no `u8g2_DrawLine` outside `ui/canvas.cpp`;
   - no `settimeofday` outside `hal/hal_clock.cpp`;
   - no `while (!Serial)` anywhere;
   - no `ESP_EXT1_WAKEUP_ALL_LOW` (deprecated alias [HWF F05]).
2. **Models are separate from views.** Models (`Stopwatch`, `Countdown`, target, `Settings`, `TimeService`, `ThermoService`, `CompassService`) are owned by `AppCore` and ticked every `step()` on every page and in every power state. Screens are **stateless renderers + binding providers**. Ephemeral UI state (cursor, pending edit, details flag, animation clocks) lives in `UiState`, owned by `UiController`.
3. **No heap in steady state.** All buffers are static or members. No STL containers. `std::array` and `<cmath>` are allowed. No exceptions, no RTTI.
4. **One thread.** Everything runs in the Arduino loop task. The only ISR is the Diagnostics DRDY counter on GPIO12, attached only while Diagnostics › Sensors is open. **No GPIO interrupts on key pins.** (If one is ever added: `gpio_wakeup_enable()` overwrites the pin's interrupt type and `gpio_wakeup_disable()` does not restore it [HWF F18], so it must be restored after every light sleep.)
5. **Every wait is a deadline.** No `delay()` outside `hal/`. Blocking calls allowed: one I2C transaction (≤ 25 ms timeout), one display page span (≤ ~5 ms), one NVS write (≤ ~10 ms [EST]), and the boot sequence in `setup()`.

### 4.3 Data flow

```
GPIO ─▶ IKeys ─▶ InputLogic ─▶ AppCore: wake/alert gate ─▶ UiController.dispatch(Bindings) ─▶ UiCommand ─▶ AppCore actions ─▶ models / settings / power
I2C0 ─▶ II2cBus ─▶ ShtDriver ─▶ ThermoService ─┐
I2C0 ─▶ II2cBus ─▶ MagDriver ─▶ CompassService ─┤
esp_timer (MonoUs) ─▶ Stopwatch / Countdown ────┤
system time (UtcMs) ─▶ TimeService ─────────────┼─▶ AppCore.buildViewModel() ─▶ ViewModel ─▶ Screen.render(Canvas) ─▶ u8g2 buffer
HealthRegistry ◀── every service ──────────────┘                ─▶ Compositor (slides) ─▶ chrome ─▶ FrameDiff ─▶ IDisplay.sendPageSpan (Wire1)
Screen.bindings ─▶ rail hints AND dispatcher AND docs/BUTTONS.md (same table)
PowerManager ◀─ activity, demand, USB, deadlines ─▶ Directive {None | Light(us) | Deep(plan)} ─▶ hal/board
Persist ◀─▶ IKvStore (NVS);  RtcState ◀─▶ IRtcStore (RTC_NOINIT) via RtcUs bridge
Logger ─▶ LogRing ─▶ ISerialPort;  ISerialPort ─▶ parseLine ─▶ Console ─▶ TimeService / Settings / diagnostics / UiController.toast
```

### 4.4 Public interfaces (contracts; namespace `hg`; C++17)

These headers are written in WP0 exactly as below (comments may be expanded, signatures may not change without updating this spec). Implementation details inside `.cpp` files are free. Blocks show declarations only: every header starts with `#pragma once`, includes what it uses (listed in the first comment line where not obvious) and wraps its content in `namespace hg { … }`.

**Include graph** ("X: Y" = X includes Y; acyclic; WP0's acceptance compiles every header alone in a one-line `.cpp`):
```
every header           : core/types.h
core/tz.h, core/cli.h  : core/civil_time.h
core/rtc_bridge.h      : core/clock_policy.h, core/stopwatch.h, core/countdown.h
ui/canvas.h            : ui/u8g2_inc.h, ui/icons.h            ui/display_port.h, ui/oled_setup.h : ui/u8g2_inc.h
ui/bindings.h          : core/input.h, ui/icons.h
ui/view_types.h        : core/types.h only (ScreenId, FormatCtx)
ui/fmt.h               : ui/view_types.h, core/settings.h, core/civil_time.h
ui/strings.h           : core/types.h (ErrCode)
ui/view_model.h        : ui/view_types.h, ui/bindings.h, core/{civil_time,clock_policy,stopwatch,countdown,heater_policy,
                         mag_calibrator,settings,input}.h        — never ui/screen.h
ui/screen.h            : ui/view_types.h, ui/bindings.h; forward-declares struct ViewModel, struct UiState, class Canvas
ui/screens/registry.h  : ui/screen.h
ui/ui_controller.h     : ui/view_model.h, ui/display_port.h, ui/canvas.h
app/ports.h            : ui/display_port.h, core/rtc_bridge.h, drivers/i2c_bus.h
```

#### core/types.h
```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace hg {
// Strong time types: no implicit conversion between them, ever.
struct MonoUs { int64_t v; };   // esp_timer_get_time(): XTAL while awake, compensated across light sleep, ~0 after any reset
struct RtcUs  { uint64_t v; };  // esp_rtc_get_time_us(): RTC counter (internal RC), continues across deep sleep and SW resets
struct UtcMs  { int64_t v; };   // wall clock, Unix epoch ms; display and logs only
constexpr int64_t kUsPerMs = 1000, kUsPerS = 1000000;
constexpr int64_t operator-(MonoUs a, MonoUs b) { return a.v - b.v; }            // µs
constexpr MonoUs  operator+(MonoUs a, int64_t us) { return MonoUs{a.v + us}; }
constexpr bool operator<(MonoUs a, MonoUs b)  { return a.v < b.v; }
constexpr bool operator<=(MonoUs a, MonoUs b) { return a.v <= b.v; }
constexpr bool operator>=(MonoUs a, MonoUs b) { return a.v >= b.v; }
constexpr bool operator==(MonoUs a, MonoUs b) { return a.v == b.v; }
constexpr int64_t operator-(RtcUs a, RtcUs b) { return (int64_t)(a.v - b.v); }   // µs
enum class Key : uint8_t { Up = 0, Down = 1, Hash = 2, Star = 3 };
constexpr uint8_t kKeyCount = 4;
constexpr uint8_t keyBit(Key k) { return (uint8_t)(1u << (uint8_t)k); }
enum class Health : uint8_t { Unknown, Ok, Degraded, Fault, Absent, Unsupported };
enum class Severity : uint8_t { Info, Warn, Fault, Fatal };
enum class Brightness : uint8_t { Low, Mid, High };
enum class ErrCode : uint8_t {           // table with mnemonic/severity/device in core/health.cpp, §9.4; texts in ui errTitle()/errHint()
  None, OledNone, OledIo, ShtNone, ShtUnknown, ShtCrc, ShtIo, Sht3xInfo, HeatSkipHot,
  MagNone, MagUnknown, MagIdWarn, MagStuck, MagSat, MagUncal, MagCalOtherChip, MagNoStatus,
  I2cStuck, I2cRecovered, KeyStuck, NvsFail, CfgReset, ClockLost, SyncRejected,
  CrashRestored, Brownout, SleepFail, RtcLost, Count };
// Device identities shared by drivers, app and ui (declared here so ui/ never includes drivers/):
enum class ShtChip  : uint8_t { None, Sht4x, Sht3x, Unknown };
enum class MagChip  : uint8_t { None, Qmc5883l, Qmc5883p, Hmc5883l, Unknown };
enum class DrdyKind : uint8_t { None, RisingLevel, FallingPulse };       // QMC5883P has no DRDY pin
struct I2cStats { uint32_t ok, absent, fail, recoveries, recoveryFails; uint8_t consecutiveFail; };
// Same numeric values as esp_reset_reason_t in IDF 5.5 (esp_system.h); hal/hal_power.cpp static_asserts each one (D36).
enum class ResetReason : uint8_t { Unknown = 0, PowerOn = 1, Ext = 2, Sw = 3, Panic = 4, IntWdt = 5, TaskWdt = 6, Wdt = 7,
                                   DeepSleep = 8, Brownout = 9, Sdio = 10, Usb = 11, Jtag = 12, Efuse = 13, PwrGlitch = 14,
                                   CpuLockup = 15 };
// Our own small set; hal maps esp_sleep_get_wakeup_cause() with an explicit switch (anything else → Other).
enum class WakeCause : uint8_t { None /*not a sleep wake*/, Ext1, Timer, Gpio /*light sleep*/, Other };
struct BatteryState { bool present; uint16_t mv; uint8_t pct; };        // IBattery result; ViewModel.bat
}
```

#### core/crc.h
```cpp
uint8_t  crc8Sensirion(const uint8_t* d, size_t n);       // poly 0x31, init 0xFF, no reflection, xorout 0
uint32_t crc32(const void* d, size_t n, uint32_t seed = 0); // IEEE (reflected 0xEDB88320): "123456789" → 0xCBF43926
uint32_t fnv1a32(const char* s, uint32_t h = 2166136261u); // "Oct 11 2026 19:30:12" → 0x1880C947
```

#### core/civil_time.h
```cpp
struct Civil { int16_t year; uint8_t month, day, hour, minute, second, weekday; };  // weekday 0=Mon..6=Sun
int64_t daysFromCivil(int y, unsigned m, unsigned d);   // Hinnant
void    civilFromDays(int64_t z, int& y, unsigned& m, unsigned& d);
uint8_t weekdayFromDays(int64_t z);
uint8_t daysInMonth(int y, unsigned m);
Civil   civilFromEpoch(int64_t epochS);                 // fields as UTC
int64_t epochFromCivil(const Civil& c);                 // fields as UTC (weekday ignored)
bool parseBuildStamp(const char* date, const char* time, Civil& out);   // "Oct 11 2026", "19:30:12"
bool parseLocalDateTime(const char* s, Civil& out);     // "YYYY-MM-DD HH:MM[:SS]" or with 'T'
bool parseUnixMs(const char* s, int64_t& ms);           // "1791736927" or "1791736927.250"
```

#### core/tz.h
```cpp
enum class DstRule : uint8_t { None, EU, US };
struct TzDef { const char* id; const char* stdAbbr; const char* dstAbbr; const char* title /*caps, picker value*/;
               const char* note /*sub-label after "UTC±h"*/; int16_t stdOffsetMin; DstRule dst; };   // §5.5; ASCII, checked by test_strings
constexpr uint8_t kTzCount = 18, kTzDefault = 7;        // index 7 = "MSK"
extern const TzDef kTzTable[kTzCount];
int     findTz(const char* id);                          // −1 if unknown
int16_t utcOffsetMin(const TzDef& tz, int64_t utcS, bool* isDst = nullptr);
Civil   utcToLocal(int64_t utcS, const TzDef& tz);
int64_t localToUtc(const Civil& local, const TzDef& tz); // spring-forward gap → +60 min; autumn overlap → earlier instant
const char* tzAbbr(const TzDef& tz, int64_t utcS);
```

#### core/clock_policy.h
```cpp
enum class ClockQuality : uint8_t { None, Approx, Manual, Synced };
enum class ClockSource  : uint8_t { None, Build, Restored, Manual, Serial };
struct ClockMeta { ClockQuality q; ClockSource src; int64_t lastSyncUtcS; int32_t lastSyncDeltaMs; };
constexpr int64_t kMinValidUtcS = 1735689600;   // 2025-01-01T00:00:00Z
constexpr int64_t kMaxValidUtcS = 4102444800;   // 2100-01-01T00:00:00Z
struct BootClockIn {
  bool rtcStateValid; ClockMeta rtcMeta;         // RtcState verified (deep sleep / SW reset survived)
  bool sysTimeValid;  int64_t sysUtcS;           // gettimeofday() within [kMin, kMax)
  bool nvsValid;      int64_t nvsUtcS; ClockMeta nvsMeta;   // NVS 'clk' checkpoint
  bool buildValid;    int64_t buildUtcS;         // parsed __DATE__/__TIME__ in kBuildTzOffsetMin + kBuildToBootLagS
  uint32_t buildId;   uint32_t nvsBuildId;       // fnv1a32 of "__DATE__ __TIME__"; NVS 'bld' (0 if absent)
};
struct BootClockOut { bool setTime; int64_t utcS; ClockMeta meta; bool storeBuildId; bool clockLost; };
BootClockOut decideBootClock(const BootClockIn& in);     // table in §5.3
enum class SyncVerdict : uint8_t { Ok, BeforeMin, AfterMax, BeforeBuild };
SyncVerdict validateSync(int64_t utcS, int64_t buildUtcS); // BeforeBuild if utc < build − 1 day
```

#### core/stopwatch.h
```cpp
class Stopwatch {
 public:
  enum class State : uint8_t { Idle, Running, Paused };
  static constexpr uint8_t kMaxLaps = 99;                         // ring of the most recent splits
  static constexpr uint16_t kMaxLapNo = 999;                      // lap numbers shown in a 3-digit cell
  static constexpr int64_t kMaxElapsedUs = 360000LL * kUsPerS - 10000; // 99:59:59.99
  void start(MonoUs t);  void pause(MonoUs t);  void resume(MonoUs t);  void reset();
  bool lap(MonoUs t);                                 // Running and lapNo < 999 only; false otherwise (→ toast "Max 999 laps")
  bool tick(MonoUs now);                              // true once when the cap auto-pauses
  State    state() const;
  int64_t  elapsedUs(MonoUs now) const;
  int64_t  currentLapUs(MonoUs now) const;            // since the last split
  uint16_t lapNo() const;                             // completed laps, keeps counting past 99 up to 999
  uint8_t  storedLaps() const;                        // min(lapNo, 99)
  int64_t  lapUs(uint8_t fromNewest) const;           // duration of a completed lap (0 = newest)
  int64_t  lapDeltaUs(uint8_t fromNewest) const;      // lapUs(i) − lapUs(i+1); 0 if no older lap
  struct Snapshot { uint8_t state; uint8_t head; uint16_t lapNo; int64_t elapsedUs; int64_t lastSplitUs; uint32_t lapMs[kMaxLaps]; };
  // (implementation note: lapMs = lap durations; lastSplitUs added because it cannot be rebuilt once > 99 laps exist)
  Snapshot save(MonoUs now) const;                    // elapsed evaluated at 'now'
  void restore(const Snapshot& s, int64_t sleptUs, MonoUs now);  // Running: elapsed = s.elapsed + sleptUs, keeps running from now
};
```

#### core/countdown.h
```cpp
class Countdown {
 public:
  enum class State : uint8_t { Select, Running, Paused, Done };
  static constexpr uint8_t  kPresetCount = 6;
  static constexpr uint16_t kPresetMin[kPresetCount] = {1, 3, 5, 10, 30, 60};
  static constexpr int64_t  kMaxTotalUs = 10LL * 3600 * kUsPerS;
  void setPreset(uint8_t i);  void nextPreset();  uint8_t preset() const;
  void start(MonoUs t);  void pause(MonoUs t);  void resume(MonoUs t);
  bool addMinute();                        // Running only: deadline and total += 60 s; false (no change) if total would exceed kMaxTotalUs → toast "Max 10 h"
  void cancel();                           // Paused → Select (preset kept)
  void restart(MonoUs t);                  // Done → Running, same preset
  void toSelect();                         // Done → Select
  bool tick(MonoUs now);                   // true exactly once on Running → Done (doneAt = deadline)
  State   state() const;
  int64_t remainingUs(MonoUs now) const;   // Running/Paused
  int64_t totalUs() const;
  int64_t overtimeUs(MonoUs now) const;    // Done: now − doneAt
  MonoUs  deadline() const;                // Running only
  static int64_t displaySeconds(int64_t remainingUs);   // ceil(remaining / 1 s)
  struct Snapshot { uint8_t state, preset; int64_t totalUs, remainingUs, overtimeUs; };
  Snapshot save(MonoUs now) const;
  void restore(const Snapshot& s, int64_t sleptUs, MonoUs now);  // Running: deadline = now + remaining − slept (may be past → next tick() fires)
};
```

#### core/rtc_bridge.h
```cpp
struct RtcState {                         // lives in RTC_NOINIT memory (hal_store), ≈ 520 B
  uint32_t magic;                         // 0x48473252 'HG2R'
  uint16_t layout;                        // kRtcLayout = 1; any change bumps it
  uint16_t size;                          // sizeof(RtcState)
  uint32_t buildId;                       // fnv1a32 of the build stamp that wrote it ("restored across update" log when ≠)
  ClockMeta clock;
  RtcUs    savedAt;                       // esp_rtc_get_time_us() when the snapshot below was taken
  Stopwatch::Snapshot sw;
  Countdown::Snapshot tm;
  int16_t  targetDeg;                     // −1 = none
  uint8_t  page;                          // carousel page
  uint8_t  alertFlags;                    // bit0 alert active, bit1 missed-alert bell
  uint8_t  lastSleep;                     // 0 none, 1 auto power-off, 2 manual
  uint8_t  stuckKeyMask;                  // keys excluded from ext1 at the last deep sleep (→ card at boot)
  uint8_t  phantomWakes;                  // phantom ext1 wakes inside the current window (§7.5)
  RtcUs    phantomWindowStart;            // RTC time of the first phantom wake of the window (2 min)
  uint8_t  crashCount;                    // consecutive crash resets without 60 s of uptime
  ShtChip  shtChip; uint8_t shtAddr;      // last detected SHT (DEEPSLEEP wakes verify it instead of a full probe, §6.2)
  RtcUs    shtBusyUntil;                  // RTC time until which a heater pulse may still run (heater ACK + 1.2 s); written at the ACK
  uint32_t crc;                           // crc32 of all bytes before this field
};
void rtcSeal(RtcState& s);                // fills magic/layout/size/crc
bool rtcVerify(const RtcState& s);        // magic, layout, size and crc all match
bool rtcMagicOnly(const RtcState& s);     // magic matches but layout/size/crc do not: a snapshot existed and is unusable
enum class RestoreVerdict : uint8_t { Fresh, Restore, RestoreCrash, Lost, CrashLoop };
RestoreVerdict decideRestore(ResetReason r, const RtcState& s);  // §5.4 table (uses rtcVerify/rtcMagicOnly/crashCount)
bool isPhantomWake(WakeCause c, uint8_t woken, uint8_t lowAtBoot, int64_t lastSleepUs);  // §7.5
int64_t sleptUs(const RtcState& s, RtcUs nowRtc);  // nowRtc − savedAt (real time between the snapshot and now), clamped to ≥ 0
```

#### core/input.h
```cpp
enum class Trigger : uint8_t { OnDown, OnClick };
struct KeyConfig { Trigger shortTrigger; uint16_t holdMs; bool repeat; bool accel; };   // holdMs 0 = no long action
struct InputEvent {
  enum class Type : uint8_t { Down, Short, Long, Repeat, Up, Stuck, Unstuck } type;
  Key key; MonoUs tDown; uint16_t heldMs; uint8_t step;   // step: 1, or 5 when accelerated
};
struct HoldState { bool active; Key key; float progress; };   // progress 0..1 from kHoldVisualMs to the latched holdMs
class InputLogic {
 public:
  void configure(Key k, const KeyConfig& c);      // from the current Bindings, before feed(); applies to the NEXT press of k only
  void setCaptureAll(bool on);                    // Diagnostics › Buttons: every key reported independently; no stuck rule
  void feed(uint8_t rawPressedMask, MonoUs now);  // bit i = Key(i) electrically pressed
  bool pop(InputEvent& out);                      // FIFO of 16; overflow drops the oldest and counts
  void swallowUntilRelease(uint8_t keyMask);      // keys of the mask that are down now: no events until released; keys already up: no effect
  HoldState hold(MonoUs now) const;
  uint8_t  downMask() const;                      // debounced, stuck keys excluded
  uint8_t  stuckMask() const;                     // keys LOW ≥ kKeyStuckMs (§2.2); a key leaves it after kKeyStuckClearMs HIGH
  uint32_t dropped() const;
};
```
Semantics are §2.2. The `KeyConfig` is latched at Down (D25). `Short` for an `OnDown` press is emitted immediately after `Down` with the same `tDown`; such a press then emits only `Repeat` (if its latched config has `repeat`) and `Up`. `Repeat` events carry `step`. `Up` is emitted for every accepted press that was not swallowed. `Stuck` and `Unstuck` (`key` set, other fields 0) are emitted once when a key enters or leaves `stuckMask()` (D26); a stuck key's press emits no `Up`.

#### core/env_math.h
```cpp
float sht4xTempC(uint16_t raw);  float sht4xRh(uint16_t raw);   // RH clamped 0..100
float sht3xTempC(uint16_t raw);  float sht3xRh(uint16_t raw);   // RH = 100·S/65535
float correctRhForTemp(float rhRaw, float tRawC, float tCorrC);  // rh·es(tRaw)/es(tCorr), clamp 0..100 (dew point kept)
float dewPointC(float tC, float rh);                             // Magnus a = 17.62, b = 243.12 °C
float absHumidity(float tC, float rh);                           // g/m³
float cToF(float c);
```

#### core/heater_policy.h
```cpp
enum class HeaterMode  : uint8_t { Off, Auto, Every5, Every10, Every30, Every60 };
enum class HeaterPower : uint8_t { Low20mW, Mid110mW, High200mW };
struct HeaterIn { HeaterMode mode; bool supported; bool busy; MonoUs now; bool havePulse; MonoUs lastPulse;
                  bool tValid; float tC; uint8_t highRhStreak;
                  bool shutdownPending; };   // deep sleep requested or the sequencer is running → never start a pulse
enum class HeaterDecision : uint8_t { No, Pulse, SkipHot, SkipUnsupported, SkipSpacing, SkipShutdown };
HeaterDecision heaterDue(const HeaterIn& in);   // rules §6.2
uint8_t heaterCommand(HeaterPower p);           // 0x1E / 0x2F / 0x39 (1 s pulses)
```

#### core/compass_math.h
```cpp
struct MagCal { uint8_t chip; float offX, offY, scaleX, scaleY, rCalCounts, lsbPerUt; uint8_t qualityPct; bool valid; };
struct Mount { uint8_t rot; bool chipDown; };     // rot: silk X arrow at 0/90/180/270° clockwise from device forward
Mount mountFromSetting(uint8_t v);                // v 0..7: rot = v & 3, chipDown = v >> 2
void  toDevice(const MagCal& c, const Mount& m, float bx, float by, float& F, float& L);  // board frame → forward/left
float headingDeg(float F, float L);               // atan2(L, F) → [0, 360)
float wrap360(float deg);  float wrap180(float deg);
const char* point16(float deg);                   // "N", "NNE", …
int16_t shownHeading(float deg, int16_t prevShown);   // round with ±0.75° hysteresis
class VectorEma { public: void reset(); void push(float F, float L, float dtS, float tauS);
                  bool valid() const; float fwd() const; float left() const; };   // impl. note: not F()/L(): Arduino's F() macro
class Spring    { public: void snap(float deg); float update(float targetDeg, float dtS, float omega);
                  float angle() const; bool settled() const; };   // settled: |ω| < 1°/s and |e| < 0.3°
```

#### core/mag_calibrator.h
```cpp
class MagCalibrator {
 public:
  enum class Verdict : uint8_t { Collecting, Ready, Failed };
  enum class Hint : uint8_t { TurnMore, KeepLevel, TurnSlower, MetalNear, FieldWeak, Ready };
  enum class Grade : uint8_t { Good, Ok, Poor };
  struct Progress { uint32_t binsMask; uint8_t coveragePct; float fieldUt; float liveDeg; Hint hint; Verdict verdict; };
  struct Result { MagCal cal; Grade grade; float fieldUt; float axisRatio; uint8_t coveragePct; Hint reason; };
  void begin(uint8_t chip, float lsbPerUt, MonoUs now);
  void add(int16_t bx, int16_t by, int16_t bz, bool overflow, MonoUs now);   // board frame, ≤ 50 Hz
  Progress progress(MonoUs now) const;
  Result   result() const;
};
```

#### core/settings.h (numeric schema only; all user-visible text lives in `ui/fmt` + `ui/strings*`)
```cpp
enum class SettingId : uint8_t { TimeZone, SetTime, ScreenOff, Brightness, Animations, AutoOff, LightSleep,
  TempUnit, TempOffset, Heater, HeaterPower, Declination, Mount, NeedleSpeed, Calibrate, Diagnostics,
  PowerOff, ResetSettings, Version, Count };                                   // 19 rows, §8
enum class SettingKind : uint8_t { Enum, Number, Action, ActionHold, Info };
enum class Section : uint8_t { Clock, Display, Power, Sensor, Compass, System };
struct SettingDef { SettingId id; Section section; SettingKind kind; const char* nvsKey;
                    int16_t def, min, max, step; const int16_t* options; uint8_t nOptions; bool accel; };
struct Settings { int16_t v[(int)SettingId::Count]; int16_t get(SettingId) const; void set(SettingId, int16_t); };
enum class NotEditable : uint8_t { No, NoSht4x, NoSensor, NoMagnetometer };
const SettingDef& settingDef(SettingId id);
Settings defaultSettings();
bool     isValid(SettingId id, int16_t v);
int16_t  sanitize(SettingId id, int16_t v);                             // invalid → default
int16_t  stepValue(SettingId id, int16_t cur, int dir, uint8_t step);   // Enum: option index ±1, clamped; Number: ±step·k, clamped
NotEditable editable(SettingId id, ShtChip sht, MagChip mag);           // e.g. Heater on SHT3x → NoSht4x
```
`ui/fmt.h` (WP7) owns the text side: `uint16_t settingLabel(SettingId)`, `void formatSettingValue(SettingId, int16_t, ValueStyle, const FormatCtx&, char*, size_t)` with `enum class ValueStyle : uint8_t { List, Picker, Cover }` (§3.8.5), `void formatSettingSubLabel(SettingId, int16_t, const FormatCtx&, char*, size_t)`. `FormatCtx` is **not** defined there: it lives in WP0's `ui/view_types.h` (below), because `ViewModel` holds one.

#### core/power_policy.h
```cpp
enum class DisplayState : uint8_t { On, Dim, Off };
struct PowerIn {
  MonoUs now, lastActivity, displayOffSince;
  uint32_t screenTimeoutMs;  uint32_t autoOffMs;            // 0 = never
  bool lightSleepSetting, usbHost, keyDown, alertActive, keepAwake, sensorBusy, animating, shutdownRequested;
  // keyDown: any NON-STUCK key down (InputLogic::downMask() != 0); a stuck key never counts (§2.2)
  bool usbRaw, usbPending;                                  // HWCDC::isPlugged() now; UsbDebouncer::pending()
  MonoUs usbProbeUntil;                                     // stay awake until then to sample USB (every 8th light-sleep wake, §7.2)
  bool timerRunning; int64_t timerRemainingUs;
  MonoUs nextDeadline;                                      // earliest of all scheduled work
};
struct PowerOut { DisplayState display; bool lightSleep; uint32_t lightSleepUs; bool autoPowerOff; };
PowerOut decidePower(const PowerIn& in);                    // rules §7.2
class UsbDebouncer { public: void feed(bool raw, MonoUs now); bool plugged() const;
                     bool pending() const; };               // on after 1 s true, off after 3 s false; starts plugged;
                                                            // pending = raw ≠ plugged() (a debounce is counting)
```

#### core/health.h
```cpp
enum class Device : uint8_t { Oled, Sht, Mag, Bus0, Bus1, Keys, Nvs, Clock, Power, Count };
struct DeviceHealth { Health h; ErrCode code; uint32_t errors; MonoUs since; };
struct ErrInfo { ErrCode code; const char* mnemonic; Severity sev; Device dev; };   // texts: ui errTitle()/errHint()
const ErrInfo& errInfo(ErrCode c);
class HealthRegistry {
 public:
  void set(Device d, Health h, ErrCode c, MonoUs now);   // logs on change
  const DeviceHealth& get(Device d) const;
  bool anyWarnOrWorse() const;  ErrCode worst() const;  uint32_t revision() const;
};
```

#### core/cli.h
```cpp
enum class Cmd : uint8_t { None, Ping, Help, Info, TimeUnix, TimeLocal, TimeQuery, TzSet, TzQuery, Scan, Sht, Heat,
                           Mag, Keys, Get, Set, Log, LogDump, Tlm, Reboot, Factory, Error };
struct CmdLine { Cmd cmd; int64_t unixMs; Civil local; char key[8]; char val[16]; int32_t num; const char* err; };
CmdLine parseLine(const char* line);   // case-insensitive, trims, ≤ 96 chars, never fails silently (Error + err text)
```

#### core/log_ring.h
```cpp
class LogRing { public: static constexpr uint8_t kLines = 48, kLen = 96;
  void push(const char* line);  uint8_t count() const;  const char* line(uint8_t oldestFirst) const; };
```

#### drivers/i2c_bus.h (the helpers are real code in WP0's `drivers/i2c_bus.cpp`, not stubs: WP4/WP5 test against them)
```cpp
enum class I2cStatus : uint8_t { Ok, Absent, Fail, Stuck };
// Absent: zero-length probe NACK (endTransmission() == 2). Fail: endTransmission 4/5/other, or requestFrom() short.
// Core 3.3.12 maps any NACK *with payload* to 4 and never returns 3 [V-src Wire.cpp; HWF sht4x F21] → no NackData state.
class II2cBus {                                   // I2cStats: core/types.h
 public:
  virtual ~II2cBus() = default;
  virtual bool      probe(uint8_t addr) = 0;                                        // zero-length write
  virtual I2cStatus write(uint8_t addr, const uint8_t* d, size_t n) = 0;            // with STOP
  virtual I2cStatus read(uint8_t addr, uint8_t* d, size_t n) = 0;                   // requestFrom; Fail if short
  virtual I2cStatus writeRead(uint8_t addr, const uint8_t* w, size_t wn, uint8_t* r, size_t rn) = 0;  // repeated START
  virtual bool      recover() = 0;                                                  // 9 SCL + STOP + re-begin
  virtual uint8_t   scan(uint8_t* found, uint8_t max) = 0;                          // 0x08..0x77
  virtual const I2cStats& stats() const = 0;
  I2cStatus writeReg(uint8_t a, uint8_t reg, uint8_t v);                            // helpers in i2c_bus.cpp
  I2cStatus writeCmd(uint8_t a, uint8_t cmd);
  I2cStatus writeCmd16(uint8_t a, uint16_t cmd);
  I2cStatus readRegs(uint8_t a, uint8_t reg, uint8_t* d, size_t n);
};
```

#### drivers/sht.h
```cpp
struct ShtSample { float tC, rh; bool hot; };            // raw conversion, no user offset
class ShtDriver {
 public:
  explicit ShtDriver(II2cBus& bus);
  struct ProbeHint { ShtChip cached = ShtChip::None; uint8_t addr = 0; uint32_t busyWaitUs = 0; };  // from RtcState (§6.2)
  void beginProbe(MonoUs now, const ProbeHint& hint = {});  // non-blocking sequence §6.2
  enum class Poll : uint8_t { Idle, Busy, Sample, Error };
  Poll tick(MonoUs now, ShtSample* out);                  // call every loop; returns Sample once per finished op
  bool startMeasure(MonoUs now);                          // false if busy/not ready
  bool startHeater(MonoUs now, uint8_t heaterCmd);        // SHT4x only; result is flagged hot
  bool probing() const;  bool busy() const;
  ShtChip chip() const;  uint8_t addr() const;  uint8_t ackAddr() const;  uint32_t serial() const;
  uint16_t crcErrors() const;  uint16_t ioErrors() const;  uint8_t consecutiveErrors() const;
};
```

#### drivers/mag.h
```cpp
struct MagSample { int16_t x, y, z; bool overflow; MonoUs at; };    // BOARD frame (HMC/QMC5883L axis convention)
class MagDriver {
 public:
  explicit MagDriver(II2cBus& bus);
  MagChip probe();                       // ≤ 5 ms: 0x0D, 0x2C, 0x1E (zero-length probe, then ID registers)
  void start(MonoUs now);                // soft reset now; configuration completes inside poll() after the reset wait
  bool standby();                        // §6.3 per chip
  bool poll(MonoUs now, MagSample* out); // status read, then burst read if ready; true on a new valid sample
  MagChip chip() const;  uint8_t addr() const;  uint8_t idByte() const;  bool idWarning() const;
  float lsbPerGauss() const;  DrdyKind drdy() const;  bool active() const;
  bool stuck() const;  uint16_t ioErrors() const;  uint16_t overflows() const;
  bool noStatus() const;                 // 0x1E chip with a dead RDY bit: timed reads every 1/ODR (W-MAG-NOSTATUS, §6.3)
};
```

#### ui/canvas.h
```cpp
// includes: ui/u8g2_inc.h, ui/icons.h
enum class Font : uint8_t { Hero, Big, BigS, Heading, Mid, Label, Value, Body, Micro, Count };
enum class Align : uint8_t { Left, Center, Right };
struct Rect { int16_t x, y, w, h; };
constexpr char    kDegChar = '`';                      // drawn as the small degree primitive (§3.3, D29)
constexpr int16_t kDegAdvance = 4;
struct DrawRecorder;                                   // host only (layout audit)
class Canvas {
 public:
  explicit Canvas(u8g2_t* g);
  void clear();  void color(uint8_t c);                // 0 clear, 1 set, 2 XOR (not for circle/disc)
  void clipContent();  void clipNone();                // content = x 0..116
  void font(Font f);
  int16_t width(const char* s) const;                  // u8g2_GetStrWidth over the runs between kDegChar + kDegAdvance each (static text)
  int16_t numWidth(const char* s) const;               // Σ advances, digits counted as '0', kDegChar = kDegAdvance
  int16_t text(int16_t x, int16_t inkTop, const char* s, Align a = Align::Left);   // kDegChar → small degree
  int16_t num(int16_t x, int16_t inkTop, const char* s, Align a = Align::Left);    // kDegChar → small degree
  void pixel(int16_t x, int16_t y);  void hline(int16_t x, int16_t y, int16_t w);  void vline(int16_t x, int16_t y, int16_t h);
  void dottedH(int16_t x, int16_t y, int16_t w);  void dottedV(int16_t x, int16_t y, int16_t h);
  void box(Rect r);  void frame(Rect r);  void rbox(Rect r, uint8_t rad);  void rframe(Rect r, uint8_t rad);
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);   // signed Bresenham, per-pixel clip
  void circle(int16_t cx, int16_t cy, uint8_t r);  void disc(int16_t cx, int16_t cy, uint8_t r);  // must fit on screen
  void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2);
  void icon(int16_t x, int16_t y, IconId id);
  void degree(int16_t x, int16_t inkTop, bool large);
  void polar(int16_t cx, int16_t cy, float r, float deg, int16_t& x, int16_t& y) const;   // 0° = up, clockwise
  uint8_t* buffer();                                   // 1024 B, page-major, LSB = top row
  uint16_t rejected() const;                           // draws refused (negative/off-screen); tests assert 0; on the device
                                                       // UiController logs once per screen per boot when it grows (§4.5 step 8)
  void setRecorder(DrawRecorder* r);
};
```

#### ui/bindings.h
```cpp
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
```

#### ui/view_types.h (WP0; the types both `view_model.h` and `screen.h` need, so neither includes the other)
```cpp
enum class ScreenId : uint8_t { Home, Stopwatch, Timer, Compass, SettingsCover, SettingsList, Picker, SetTime,
                                Calibration, Diagnostics, Card, Alert, PowerOff, Splash, Count };
struct FormatCtx { ShtChip sht; MagChip mag; float liveHeadingDeg; float liveTempC; int64_t utcS;    // for ui/fmt (WP7)
                  bool haveHeading, haveTemp; int8_t calPct /*-1 none*/; uint8_t calGrade; };   // impl. note: added for list/picker texts
```

#### ui/screen.h
```cpp
// includes: ui/view_types.h, ui/bindings.h
struct ViewModel; struct UiState; class Canvas;           // forward declarations: screen.h never includes view_model.h
class Screen {
 public:
  virtual ~Screen() = default;
  virtual void bindings(const ViewModel& vm, const UiState& ui, Bindings& out) const = 0;
  virtual void render(Canvas& c, const ViewModel& vm, const UiState& ui, MonoUs now) const = 0;  // content only
  virtual uint16_t framePeriodMs(const ViewModel& vm, const UiState& ui) const = 0;   // 0 = on change only
  virtual uint8_t  sensorDemand(const ViewModel& vm, const UiState& ui) const { return 0; } // bit0 mag, bit1 fast SHT
};
```

#### ui/screens/registry.h (WP0; how `UiController` gets the screens of WP8/WP9)
```cpp
const Screen& screenFor(ScreenId id);   // registry.cpp (WP0): screenForA(id), else screenForB(id), else a null screen that
                                        // draws nothing and binds nothing (and logs once)
const Screen* screenForA(ScreenId id);  // registry_a.cpp (WP8): Home, Stopwatch, Timer, Alert, Card, PowerOff, Splash; else nullptr
const Screen* screenForB(ScreenId id);  // registry_b.cpp (WP9): Compass, Calibration, SettingsCover, SettingsList, Picker,
                                        // SetTime, Diagnostics; else nullptr
```
Screens are static objects with no state; WP0 ships `registry_a.cpp`/`registry_b.cpp` returning `nullptr`, which WP8/WP9 then own.

#### ui/view_model.h (abridged field lists; WP0 writes them in full from this list)
```cpp
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
                     uint16_t keyPresses[kKeyCount]; uint8_t keyDownMask, keyStuckMask; uint32_t serialDropped; };
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
```

#### ui/ui_controller.h
```cpp
class UiController {
 public:
  UiController(Canvas& c, IDisplay& d);
  void bindings(const ViewModel& vm, Bindings& out) const;                 // of the top-most interactive layer
  UiCommand dispatch(const InputEvent& e, const Bindings& b, const ViewModel& vm, MonoUs now);  // UI-only actions handled here
  void raiseAlert(MonoUs now);  void dismissAlert(MonoUs now);
  void toast(uint16_t str, const char* arg, MonoUs now);   // enqueues (§2.3.7); merges an identical consecutive toast
  void showCard(ErrCode c, uint8_t arg = 0);               // queued; arg = key index (KEY STUCK) or variant (STATE LOST)
  void openCalibration();  void openPowerOff(MonoUs now);  void revertEdits();
  bool wantsFrame(const ViewModel& vm, MonoUs now) const;
  void render(const ViewModel& vm, MonoUs now);                            // content (+slide) + chrome into the u8g2 buffer
  bool flush(uint32_t budgetUs, MonoUs now);                               // FrameDiff → IDisplay::sendPageSpan; true when complete
  void forceFullRefresh();
  uint8_t sensorDemand(const ViewModel& vm) const;
  bool keepAwake() const;  bool animating(MonoUs now) const;  MonoUs nextFrameAt() const;
  const UiState& state() const;  UiState& stateForTest();
};
```
`dispatch()` uses the `Binding` captured at each key's Down for that press's later events (D25); it obtains screens only through `screenFor()`.

#### ui/strings.h (WP0; frame only — the fragments carry the text)
```cpp
enum class StrRole : uint8_t { Toast, CardTitle, CardRow, Row, Label, Micro, Free };  // test_strings maps role → font + max width
enum StrId : uint16_t {
#define HG_STR(id, role, en, worst) id,
  S_NONE,
#include "strings_core.h"
#include "strings_screens_a.h"
#include "strings_screens_b.h"
#undef HG_STR
  S_COUNT };
const char* str(uint16_t id);          // table built the same way in strings.cpp (WP0)
StrRole     strRole(uint16_t id);
const char* strWorstArg(uint16_t id);  // worst-case argument for a "%s" template ("" if none); used by test_strings
uint16_t errTitle(ErrCode c);          // declared here (WP0), defined in ui/err_text.cpp (WP7)
uint16_t errHint(ErrCode c);
```

#### ui/display_port.h (WP0; owned by ui/, so ui/ never includes app/)
```cpp
// includes: ui/u8g2_inc.h, core/types.h
struct IDisplay {
  virtual bool ok() const = 0;                                   // false → headless (no frames)
  virtual u8g2_t* u8g2() = 0;
  virtual void sendPageSpan(uint8_t page, uint8_t tx0, uint8_t tw) = 0;   // u8g2_UpdateDisplayArea(tx0, page, tw, 1)
  virtual void setPower(bool on) = 0;  virtual void setLevel(Brightness b) = 0;  virtual void setContrastRaw(uint8_t v) = 0;
  virtual void setInvert(bool on) = 0;  virtual void deepOff() = 0;            // 0xAE + 0x8D 0x10
  virtual bool retryInit() = 0;                                  // zero-length probe first; full init only on ACK (§6.6)
  virtual uint32_t ioErrors() const = 0;
};
```

#### ui/oled_setup.h (header WP0; `oled_setup.cpp` and its test WP12; host-compilable so the init order is unit-tested, §6.6)
```cpp
// includes: ui/u8g2_inc.h, config/build_config.h (OledController)
// Order is fixed: u8g2_Setup_…(R0) → u8x8_SetI2CAddress(addr7 << 1) → u8g2_InitDisplay → u8g2_SetFlipMode(flip).
// (Setup resets i2c_address to 255, which CAD_INIT turns into 0x78, so the address must be set after Setup [V-src].)
void oledSetup(u8g2_t* g, OledController c, uint8_t addr7, bool flip, u8x8_msg_cb byteCb, u8x8_msg_cb gpioDelayCb);
```

#### app/ports.h
```cpp
#include "ui/display_port.h"      // IDisplay is NOT redefined here
#include "core/rtc_bridge.h"      // RtcState
#include "drivers/i2c_bus.h"      // II2cBus
struct IMonoClock { virtual MonoUs now() const = 0; };
struct IRtcClock  { virtual RtcUs  now() const = 0; };
struct IWallClock { virtual bool get(UtcMs* out) const = 0; virtual bool set(UtcMs t) = 0; };
struct IKeys      { virtual uint8_t rawPressedMask() = 0; };      // logical key bits, polarity applied
struct IKvStore { virtual bool ok() const = 0;
  virtual bool get(const char* key, void* buf, size_t len) = 0;  // true only if the key exists with exactly len bytes
  virtual bool put(const char* key, const void* buf, size_t len) = 0;  virtual bool erase(const char* key) = 0;
  virtual bool eraseAll() = 0; };
struct IRtcStore  { virtual RtcState& state() = 0; };             // RTC_NOINIT object
struct WakeInfo   { ResetReason resetReason; WakeCause cause; uint64_t ext1Mask; uint8_t levelsAtBoot /*logical keys LOW*/;
                     uint32_t sampleUs; };                         // esp_timer time of the boot key sample (§4.6 step 2)
struct DeepSleepPlan { uint64_t ext1Mask; uint64_t timerUs; bool keepRtcPeriph; };
enum class SleepResult : uint8_t { Ok, Rejected, TooShort, Error };
struct IPower {
  virtual WakeInfo wakeInfo() const = 0;  virtual bool usbHostRaw() = 0;         // HWCDC::isPlugged()
  virtual SleepResult lightSleep(uint32_t us, uint8_t excludeKeys) = 0;          // keys + timer wake; stuck keys excluded; §7.4
  virtual SleepResult deepSleep(const DeepSleepPlan& p) = 0;                     // returns only on failure; §7.5
  virtual void restart() = 0;  virtual uint32_t heapFree() const = 0;
};
struct ISerialPort { virtual size_t read(char* buf, size_t max) = 0;
                     virtual size_t write(const char* s, size_t n) = 0;          // bytes accepted; short when the TX ring is full
                     virtual size_t writable() = 0;                              // HWCDC::availableForWrite()
                     virtual bool hostConnected() = 0; virtual void flushBeforeSleep() = 0; };
struct IBattery    { virtual BatteryState read() = 0; };                        // core type: hal never depends on ui
struct IDrdy       { virtual void enable(DrdyKind k) = 0; virtual void disable() = 0; virtual uint32_t edges() const = 0; };
struct Ports { IMonoClock* mono; IRtcClock* rtc; IWallClock* wall; IKeys* keys; IDisplay* display; IKvStore* kv;
               IRtcStore* rtcStore; IPower* power; ISerialPort* serial; IBattery* battery; IDrdy* drdy;
               II2cBus* sensorBus; II2cBus* oledBus; };
```

#### app/app_core.h
```cpp
struct BootInputs { const char* buildDate; const char* buildTime; const char* coreVersion = nullptr; };   // __DATE__, __TIME__ from the .ino; impl. note: core version for the boot report
struct Directive { enum class Sleep : uint8_t { None, Light, Deep } sleep; uint32_t lightUs; uint8_t excludeKeys /*stuck keys*/;
                   DeepSleepPlan deep; uint16_t idleMs; };
class AppCore {
 public:
  explicit AppCore(const Ports& p);
  void boot(const BootInputs& in);           // §4.6 steps 5–12
  Directive step();                          // one loop iteration; never blocks > ~10 ms
  void onDeepSleepFailed(SleepResult r);     // re-init buses/display, toast "Sleep failed", log E-PWR-SLEEP, stay awake
  void noteLightSleep(SleepResult r);        // counters for Diagnostics; Rejected/TooShort = immediate wake
  const ViewModel& viewModel() const;        // host tests
};
```
`TimeService`, `ThermoService`, `CompassService`, `PowerManager`, `Persist`, `Console` and `Logger` are internal to `app/`. Their headers are owned by WP11 and are not a cross-package contract.

### 4.5 Main loop and scheduling (cooperative, single thread)

```
boardLoop():                                     (hal/board.cpp)
  Directive d = app.step();
  switch (d.sleep) {
    case Light: app.noteLightSleep(power.lightSleep(d.lightUs, d.excludeKeys)); break;   // returns on key or timer
    case Deep:  r = power.deepSleep(d.deep); app.onDeepSleepFailed(r); break;   // returns only on failure
    default:    if (d.idleMs) delay(d.idleMs);                 // ≤ 5 ms; vTaskDelay lets idle tasks run
  }

AppCore::step():
  now = mono.now()
  1 keys      input.configure(bindings) (next presses only, D25) ; input.feed(keys.raw, now) ; events → gate:
                 Stuck/Unstuck → card/log/toast (§2.2) ;
                 display Dim/Off → swallow + wake ; alert active → swallow + dismiss ; else ui.dispatch → UiCommand → act()
  2 models    stopwatch.tick ; if countdown.tick → alert (ui.raiseAlert, power.forceOn, log) ; snapshot RtcState if any model changed
  3 sensors   thermo.tick(now, displayState, page) ; compass.tick(now, ui.sensorDemand) ; battery every 30 s
  4 serial    read ≤ 96 chars/line → parseLine → console ; logger.pump(): write pending output up to serial.writable() (§7.3) ;
              start the log-ring replay when hostConnected goes false → true
  5 health    re-probe due devices (display on only), back-off 5 → 10 → 30 → 60 s
  6 persist   clock checkpoint every hour awake ; pending NVS writes
  7 power     usb.feed(isPlugged) ; PowerIn{usbRaw, usbPending, usbProbeUntil, keyDown = downMask() != 0} ; decidePower(...)
              → display state (fades) ; shutdown sequencer ; light/deep directive (lightSleep excludes input.stuckMask())
  8 frame     if display on: if ui.wantsFrame → buildViewModel → ui.render ; ui.flush(budget 8 ms) — at most 2 page spans ;
              if canvas.rejected() grew during this render → log "W UI   <screen>: N draws rejected" once per screen per boot
  9 idle      idleMs = clamp(nextDeadline − now, 0, 5)
```

| Task | Cadence | Notes |
|---|---|---|
| Key sampling | every iteration (≤ ~10 ms apart) | GPIO wake from light sleep |
| SHT measure | 2 s while Home is visible and the display is on; 10 s on other pages; **none while dark**, except Auto heater mode (read every 60 s to evaluate RH) and scheduled heater pulses | immediate read on display wake |
| Magnetometer | poll status every 20 ms while Compass, Calibration or Diag pages 1/4 are visible and the display is on; standby 2 s after the demand ends and always before deep sleep | |
| Frame: Home | when the displayed second changes | dirty spans only |
| Frame: Stopwatch running | 20 fps (50 ms) | |
| Frame: Timer running | when the displayed second or a progress pixel changes; paused blink edges | |
| Frame: Compass | 25 fps while the spring is not settled, 5 fps when settled | |
| Frame: Calibration | 20 fps | |
| Frame: Diagnostics | 5 fps (Buttons page: on every key change) | |
| Frame: slides, toasts, fades | ~30 fps, time-based | |
| Frame: static pages | on change + minute rollover of the status clock | |
| Full refresh | every 60 s while on, and after every display power-on | self-heal |
| Clock checkpoint (NVS `clk`) | on sync/set, every 60 min awake, before deep sleep | |
| RtcState snapshot | on every model change and before sleep | memcpy + CRC, cheap |

**Flush budget.**
- U8g2's SSD1306 path sends a page as 1 command + 6 data transactions (≤ 25 bytes each) [HWF u8g2 U16].
- A full frame is 1184 bytes on the wire, at least 26.8 ms at 400 kHz. One full page span is about 3.4 ms plus IDF per-transaction overhead [EST].
- `UiController::flush()` diffs the 1 KB frame against a shadow copy page by page. For each changed page it finds the first and last changed 8-px tile, calls `sendPageSpan(page, tx0, tw)` and copies those bytes into the shadow.
- At most **2 spans per loop iteration**, so keys are sampled at least every ~10 ms. A new render may start before the previous frame is fully flushed: the shadow always reflects the panel, so the diff stays correct.

### 4.6 Boot sequence (`boardSetup()`; total ≤ ~1.5 s cold, ≤ ~0.3 s warm [EST])

1. `Serial.setTxBufferSize(kSerialTxBufBytes = 4096); Serial.setTxTimeoutMs(0); Serial.begin(115200);`. The buffer size must be set **before** `begin()`: `HWCDC::begin()` creates a 256-byte TX ring only if none exists [V-src HWCDC.cpp 421–425, 512–526]; `setTxTimeoutMs`/`begin` are [V-build], `setTxBufferSize` is [V-src] (WP12 adds it to the probe sketch). Never `while (!Serial)`.
2. Read `esp_reset_reason()`, `esp_sleep_get_wakeup_cause()` and `esp_sleep_get_ext1_wakeup_status()` (mapped to `ResetReason`/`WakeCause`, D36). For every key pin: `rtc_gpio_hold_dis`, `rtc_gpio_deinit`, `pinMode(INPUT_PULLUP)`, `gpio_sleep_sel_dis` [V-build]. Sample key levels → `WakeInfo.levelsAtBoot`, and record `WakeInfo.sampleUs = esp_timer_get_time()` at that sample (shown in Diagnostics › System and the boot report; tunes §7.5).
3. `setCpuFrequencyMhz(kCpuMhz = 80)` [V-build]. APB stays at 80 MHz, so USB and I2C are unaffected [HWF F30/F31].
4. If `kForceLed48Off`: `rgbLedWrite(48, 0, 0, 0)` (WS2812 off, line LOW → red LED off) [HWF SM-09] [HW?].
5. `AppCore::boot()`: load and verify `RtcState` and compute `decideRestore()` (§5.4) now, so that steps 7 and 9 can use the verdict; open NVS namespace `hg2`; load and sanitize settings.
6. I2C (in `hal_i2c`): stuck-line check, then `Wire.begin(8, 7, 100000); Wire.setTimeOut(20); Wire1.begin(10, 11, 400000); Wire1.setTimeOut(20);` [V-build]. `Wire.begin()` without pins would use SCL = 9 [HWF u8g2 U34].
7. Display (in `hal_oled`): probe 0x3C, then 0x3D. `oledSetup()` (§6.6): setup → I2C address → init (fast if warm) → flip; then brightness. Then:
   - cold boot → splash;
   - EXT1 wake → last page directly;
   - TIMER wake → display stays off, **unless** the verdict is not `Restore` or the restored countdown is neither Running nor Done: then the display turns on for the TIMER LOST card (D32).
8. Clock: `decideBootClock()` (§5.3).
9. Sensors: SHT probe with the `RtcState` hint (DEEPSLEEP: verify the cached chip with one measurement; warm reset during a heater pulse: wait until `shtBusyUntil`; §6.2), polled from `setup()` until done or 1.5 s; magnetometer probe + standby; I2C scan (logged).
10. Models: apply the verdict (§5.4) using `sleptUs()`. If `RtcState.buildId` ≠ this build's id, log `I BOOT restored across update`.
11. Boot report → log ring + Serial (§9.2), paced by the `Logger` (§7.3). Queue cards: CHECK HARDWARE if any FAULT, KEY STUCK if `stuckKeyMask` or phantom exclusion, STATE LOST/TIMER LOST for `Lost`/`CrashLoop`. Queue toasts: "Restored after crash" (`RestoreCrash`), "Clock approx, sync USB", "Clock lost: sync or set". The toast queue stays paused while the splash or a card is up (§2.3.7), so none of them is lost.
12. Swallow keys that read LOW now (`levelsAtBoot`); a woken key that is already up is not swallowed (§2.3.2). Phantom-wake guard (§7.5).
13. `enableLoopWDT()` (TWDT 5 s [V-src sdkconfig `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`]).

### 4.7 The sketch file (complete)

```cpp
// Hiking_Gadget.ino — composition only; all code lives in src/ (ctags is stubbed: setup()/loop() only)
#include <U8g2lib.h>       // makes the Arduino builder add U8g2 to the include path for src/
#include <Wire.h>
#include <Preferences.h>
#include "src/hal/board.h"
void setup() { hg::boardSetup(__DATE__, __TIME__); }   // build stamp captured here (§5.2)
void loop()  { hg::boardLoop(); }
```

### 4.8 Host compilability (the same UI code renders on the PC)

- `ui/u8g2_inc.h`: with `ARDUINO` → `#include <clib/u8g2.h>` (impl. note: not `U8g2lib.h`, which pulls `Arduino.h` and its `F()` macro into `ui/`; the `.ino` still includes `U8g2lib.h` so the builder adds the library path); otherwise → `#include "u8g2.h"` (`-I $(U8G2)/clib`).
- **Device:** `oledSetup(&u8g2, kOledController, addr, kOledFlip, hg_byte_wire1_cb, hg_gpio_delay_cb)` → `u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, …)` (§6.6).
- **Host:** the *same* setup function with `u8x8_byte_empty` and `u8x8_dummy_cb` [HWF u8g2 U19] for rendering, and with a recording byte callback in `test_oled_setup`. Same 1024-byte page-format buffer. PBM via `u8g2_WriteBufferPBM` or a direct dump.
- The host links the **installed** U8g2 2.36.19 clib (`/dev/shm/arduino-esp32/user/libraries/U8g2/src/clib`, fallback `/mnt/wd/arduino-esp32/user/libraries/U8g2/src/clib`). Never a separately downloaded copy.

### 4.9 Memory and flash budget [EST unless noted]

- Flash: the probe sketch with Wire, Preferences, U8g2 and the sleep APIs is 357 KB [V-build]. App code ≈ 80–120 KB. Fonts 7.4 KB [V-render]. Total ≈ 450–500 KB of the 1,310,720 B app partition [HWF SM-21].
- RAM: u8g2 buffer 1 KB, shadow 1 KB, slide snapshot 1 KB, log ring 4.6 KB, logger TX FIFO 1 KB, calibrator ring 360 × 6 B ≈ 2.2 KB; HWCDC TX ring 4 KB (heap, allocated once in `setup()`, D31).
- RTC memory: `RtcState` ≈ 520 B (99 splits × 4 B + headers).

### 4.10 Strings (one table, several owners, no merge conflicts)

All user-visible text lives in `ui/strings*.h` as X-macro fragments. WP0 creates the three fragments **empty** (so `strings.h` compiles on day one); from then on each fragment has exactly one owner: `strings_core.h` WP7, `strings_screens_a.h` WP8, `strings_screens_b.h` WP9. The frame `ui/strings.h` is in §4.4.

```cpp
// ui/strings_screens_a.h (owned by WP8)       columns: id, role, English text, worst-case argument for "%s" ("" if none)
HG_STR(S_HOME_DEW,        Row,   "DEW",                "")
HG_STR(S_HOME_NO_SENSOR,  Row,   "No temp sensor",     "")
HG_STR(S_HINT_HOLD_RESET, Toast, "Hold # to reset",    "")
HG_STR(S_TOAST_PLUS1,     Toast, "+1 min, %s left",    "10:00:00")
HG_STR(S_TOAST_TARGET,    Toast, "Target %s",          "359`")      // '`' = kDegChar, drawn as the degree sign
```
- **ASCII only**, printable 0x20..0x7E; the degree sign is `kDegChar` (the backtick byte 0x60), never U+00B0 (D29).
- Roles and limits checked by `test_strings` (font, max width in px): `Toast` Body ≤ 105; `CardTitle` Label ≤ 105; `CardRow` Body ≤ 116 (≤ 108 when it follows an icon); `Row` Body ≤ 116; `Label` Label ≤ 116; `Micro` Micro ≤ 116; `Free` (positioned text with its own limit in §3.8) glyph coverage only. A template is measured with `worst` substituted for `%s`.
- Error texts: `errTitle(ErrCode)`/`errHint(ErrCode)` are declared in `ui/strings.h` (WP0) and defined in `ui/err_text.cpp` (WP7) from `S_ERR_*` ids in `strings_core.h`.
- Setting labels and value texts: `ui/fmt.cpp` (WP7), in the three `ValueStyle`s (§4.4 core/settings.h note).
- A Russian pack (Q1) replaces the `en` column with an `ru` column and four font roles (§1.4).
- `test_strings` checks every string against the glyph coverage of the font that draws it, the ASCII rule and its role's width limit; the device logs any `Canvas::rejected()` growth (§4.5), so an overflow that slips through is not silent.

---

## 5. Time

### 5.1 Three clocks, strictly separated

| Clock (type) | Source | Survives | Used for | Never used for |
|---|---|---|---|---|
| `MonoUs` | `esp_timer_get_time()` [V-build] | Light sleep (IDF advances it by the RC-measured sleep time [HWF F21]). Restarts at ~0 after **every** reset and deep sleep. | Stopwatch, timer, debounce, timeouts, animations, sensor cadence, power policy | Wall time |
| `UtcMs` | system time via `gettimeofday`/`settimeofday`, **only** in `hal/hal_clock.cpp` [V-build] | Light sleep, deep sleep, software reset (RTC timer + boot-time registers [HWF F25]). **Lost on power-on.** Note: `CONFIG_LIBC_TIME_SYSCALL_USE_RTC_HRT=y` and `CONFIG_ESP_TIME_FUNCS_USE_RTC_TIMER=y` + `…_USE_ESP_TIMER=y` [V-src sdkconfig]. | Display, "ends at", logs | Any interval |
| `RtcUs` | `esp_rtc_get_time_us()` (`esp_rtc_time.h`) [V-build] | Deep sleep and all resets except power-on | Only to compute how long the device was off between a model snapshot and the next boot (§5.4) | Anything while awake |

Rules:
- Changing the time or the zone never touches a running interval (v1 pitfall 1).
- The strong types make mixing them a compile error. There is no conversion function from `UtcMs` to `MonoUs`.
- The only `RtcUs` arithmetic is `sleptUs()` in `core/rtc_bridge`.
- **Accuracy caveat.** The RTC slow clock is the internal RC oscillator, about 136 kHz (`CONFIG_RTC_CLK_SRC_INT_RC=y`, `CONFIG_RTC_CLK_CAL_CYCLES=576` [V-src]). The SuperMini has no 32 kHz crystal. Every second spent in light or deep sleep is measured by the RC, which drifts with temperature [HWF F24]. Drift is unknown [HW?] and measured in §11 step 16. The sync reply reports the accumulated error, and the details card shows the sync age. Future hook (off in v2.0): `clock_trim_ppm` learned from two syncs.

### 5.2 Sources and quality

`ClockQuality { None, Approx, Manual, Synced }`, `ClockSource { None, Build, Restored, Manual, Serial }`.

| Source | Quality | How |
|---|---|---|
| Serial `TIME <unix>[.ms]` (from `sync_time.py` or `hg_sync.html`) | **Synced** | Primary: "time from the computer". Works at any time, not only during a boot window (v1 had a 3 s window). |
| Serial `TIME YYYY-MM-DD HH:MM[:SS]` typed in the Arduino Serial Monitor (local time in the current zone) | Manual | No tools needed. Accepts CR, LF or CRLF. |
| Settings › Set time | Manual | The only option in the field. Seconds become 00. |
| Build stamp `__DATE__ __TIME__`, captured in the `.ino` | Approx (Build) | Read as local time in `kBuildTzOffsetMin = +180` (the user's PC is on MSK), plus `kBuildToBootLagS = 30` s for compile and upload. Used **only when** `fnv1a32("__DATE__ __TIME__")` differs from NVS `bld`, so rebooting old firmware never brings back a stale time (C). arduino-cli regenerates and recompiles the sketch file on every build [HW?: confirm with the Arduino IDE in §11 step 2]. |
| NVS checkpoint `clk` (after power loss) | Approx (Restored) | Written on sync, on set, every 60 min awake and before deep sleep. The off-time is unknown, so the value is flagged. |

**How quality is shown:**
- Synced or Manual: nothing extra.
- Approx: `SYNC?` tag on Home (folded into ⚠ when the top line is full, §3.4); one toast after boot "Clock approx, sync USB"; Details row `Approx: build` / `Approx: restored`; Serial hint line.
- None: Home shows `--:--` and the date line reads `Clock not set`; the Details Clock row says `Not set`; the Serial hint line says how to sync.
- Sync age is always in Details (`Synced 2h ago`) and Diagnostics. It matters because of RC drift.

### 5.3 Boot decision (`decideBootClock`, pure, every row unit-tested)

| # | Condition | Result |
|---|---|---|
| 1 | `rtcStateValid && sysTimeValid` (deep-sleep wake or software reset) | keep system time; meta = `rtcMeta` |
| 2 | `!rtcStateValid && sysTimeValid` (rare: RTC_NOINIT damaged, time survived) | keep system time; Approx/Restored |
| 3 | otherwise, candidates = {build if `buildValid && buildId ≠ nvsBuildId`} ∪ {NVS if `nvsValid`} | newest candidate → `setTime`; Approx with source Build or Restored; `storeBuildId` whenever the build candidate existed; `clockLost = true` if the source is Restored |
| 4 | no candidates | None (`--:--`); `clockLost = true` |

`clockLost` raises `W-CLK-LOST` (WARN: status ⚠ + toast "Clock lost: sync or set", 88 px; the full sentence goes to Serial and the README).

### 5.4 Persistence of models and clock across sleep and resets

| What | Where | Written |
|---|---|---|
| Clock meta (quality, source, last sync, last delta) | `RtcState.clock` + NVS `clk` | on change; `clk` also every 60 min and before deep sleep |
| Stopwatch, countdown, target, page, alert flags | `RtcState` (RTC_NOINIT, CRC32) | on every model change and before any sleep |
| Settings, calibration, last timer preset, build id | NVS (§8.2) | on change |

**Restore rule** (`decideRestore`, pure; `ResetReason` values mirror `esp_reset_reason_t`, D36):

| Reset reason | RtcState | Verdict → action |
|---|---|---|
| `DeepSleep` | verified | `Restore`: restore everything silently |
| `DeepSleep` | not verified (CRC damage, or a `kRtcLayout` bump after an update) | `Lost`: defaults, **display on**, STATE LOST card (TIMER LOST when the wake cause is Timer), `E-RTC-LOST` logged (D32). We always seal before deep sleep, so a deep-sleep wake without a valid snapshot is never normal. |
| `Sw` (our reboot), `Usb` (IDE upload / USB-Serial/JTAG reset), `Jtag` — **non-crash** | verified | `Restore` silently; `crashCount` untouched. If `buildId` differs: log `I BOOT restored across update` (the snapshot came from the previous firmware with the same layout). |
| `Panic`, `IntWdt`, `TaskWdt`, `Wdt`, `CpuLockup` — **crash** | verified, `crashCount < 3` | `RestoreCrash`: restore, `crashCount++`, toast "Restored after crash" (`W-RST-CRASH`; the exact reason is in the log and Diagnostics). `crashCount` resets after 60 s of uptime. |
| crash reasons | verified, `crashCount ≥ 3` | `CrashLoop`: defaults, STATE LOST card (crash-loop variant), `W-RST-CRASH` logged |
| `Sw`, `Usb`, `Jtag`, crash reasons | magic matches, rest not verified (`rtcMagicOnly`) | `Lost`: defaults + STATE LOST card (a snapshot existed and is unusable) |
| same | magic does not match | `Fresh`: defaults, no card |
| `PowerOn`, `Brownout`, `Ext`, `Sdio`, `Efuse`, `PwrGlitch`, `Unknown` | — | `Fresh`: defaults (RTC memory is garbage after power-on); brownout adds `W-BROWNOUT` |

**Bridge arithmetic.** `AppCore::snapshotAll()` saves every model with the same `now` and stores `savedAt = rtc.now()` in the same call, so one `savedAt` is valid for all snapshots. At boot, `slept = max(0, rtcNow − savedAt)` (`sleptUs()`) is the real time between that snapshot and now, whether the gap was deep sleep, a crash or awake time before a crash. Then:
- Stopwatch Running: `elapsed = snapshot.elapsed + slept`, and it continues running from `now`.
- Countdown Running: `remaining = snapshot.remaining − slept`. If `≤ 0`, the deadline lies in the past and the next `tick()` fires the alert exactly once, with overtime = −remaining.
- Paused and Done states restore as they were; Done overtime += slept.
- Unit test `test_rtc_bridge`: a 10-minute simulated deep sleep with a running stopwatch and timer; timer expiry during sleep; every row of the verdict table (incl. `Usb`/`Sw` non-crash and the layout bump → `Lost`); crash-loop guard.

### 5.5 Time zones (`core/tz.cpp`; NVS stores the stable **id**, so reordering is safe)

One format, used by `fmt` (WP7) and the picker (WP9) alike:
- **Picker value** (`Value`, C@58): the title in caps (`MOSCOW`); over 110 px → `Label` (only `YEKATERINBURG`, 124 → 89 px).
- **Picker sub-label** (Body, C@58 ink 39): `UTC±h[:mm]` of the **current** offset (DST-aware), one space, then the note column (`UTC+3 Minsk, Istanbul`). Widest: `UTC+8 Beijing, Singapore` 106 px.
- **Neighbour items** above/below (Body): title case + current offset (`Kaliningrad UTC+2`); the `UTC` row reads just `UTC`. Widest: `Yekaterinburg UTC+5` 91 px.
- **Home / list / cover label**: the abbreviation column (`MSK`, `CEST`, `+07`). All widths [V-meas].

| idx | id | Home label (std/dst) | Picker value | Sub-label note (after `UTC±h`) | Std offset | DST |
|---|---|---|---|---|---|---|
| 0 | LAX | PST / PDT | LOS ANGELES | US DST | −480 | US |
| 1 | NYC | EST / EDT | NEW YORK | US DST | −300 | US |
| 2 | UTC | UTC | UTC | No DST | 0 | — |
| 3 | LON | GMT / BST | LONDON | EU DST | 0 | EU |
| 4 | CET | CET / CEST | BERLIN | Paris, Rome | +60 | EU |
| 5 | EET | EET / EEST | HELSINKI | Kyiv, Riga | +120 | EU |
| 6 | KGD | EET | KALININGRAD | No DST | +120 | — |
| **7** | **MSK** | **MSK** | **MOSCOW** | **Minsk, Istanbul** (default) | **+180** | — |
| 8 | SAM | SAMT | SAMARA | Dubai, Baku | +240 | — |
| 9 | YEK | YEKT | YEKATERINBURG | Tashkent, Almaty | +300 | — |
| 10 | DEL | IST | DELHI | India | +330 | — |
| 11 | OMS | OMST | OMSK | Bishkek | +360 | — |
| 12 | NSK | +07 | NOVOSIBIRSK | Krasnoyarsk | +420 | — |
| 13 | IRK | +08 | IRKUTSK | Beijing, Singapore | +480 | — |
| 14 | YAK | +09 | YAKUTSK | Tokyo, Seoul | +540 | — |
| 15 | VLA | +10 | VLADIVOSTOK | No DST | +600 | — |
| 16 | MAG | +11 | MAGADAN | Sakhalin | +660 | — |
| 17 | PET | +12 | KAMCHATKA | No DST | +720 | — |

- **DST** is our own pure function of UTC (newlib `TZ`/`setenv` is never used):
  - **EU:** +60 min from the last Sunday of March 01:00 UTC to the last Sunday of October 01:00 UTC.
  - **US:** +60 min from the second Sunday of March at 02:00 local standard time to the first Sunday of November at 02:00 local daylight time.
  - Russia, Belarus, Turkey and Kazakhstan have no DST. Kazakhstan has been unified on UTC+5 since 2024-03-01. Rules are table data, so a political change is a one-line edit.
- `localToUtc` (Set time, typed `TIME` with a date): a spring-forward gap is shifted +60 min; an autumn overlap resolves to the earlier instant.
- The `Value` → `Label` fallback is the generic picker rule of §3.8.5, not a time-zone special case.

**Test vectors** (computed in this session):

| Case | Value |
|---|---|
| `kMinValidUtcS` | 1735689600 = 2025-01-01 00:00:00Z |
| Example | 1791736927 = 2026-10-11 16:42:07Z = **19:42:07 MSK, Sunday** |
| EU start / end 2026 | 1774746000 (03-29 01:00Z) / 1792890000 (10-25 01:00Z) |
| US start / end 2026 (NYC) | 1772953200 (03-08 07:00Z) / 1793512800 (11-01 06:00Z) |
| EU start / end 2027 | 1806195600 / 1824944400 |
| US start / end 2027 | 1805007600 / 1825567200 |
| Build stamp `"Oct 11 2026" "19:30:12"` at +180 | 1791736212 (+30 s lag → 1791736242) |
| `fnv1a32("Oct 11 2026 19:30:12")` | 0x1880C947 |

Each boundary is tested at ±1 s.

### 5.6 Sync protocol and tools

**Serial commands** (full list in §9.3). Every reply starts with `OK` or `ERR <reason>`; **every `ERR` also shows a toast** with the reason.

| Request | Accepted reply | Rejections (each → toast) |
|---|---|---|
| `PING` | `PONG HG2 2.0.0 <uptime_ms>` | — |
| `TIME 1791736927.250` | `OK TIME utc=2026-10-11T16:42:07.250Z local=19:42:07 MSK delta=-1.84s age=3d04h q=SYNCED` (toast "Time set -1.8 s") | `ERR TIME BEFORE_MIN` / `AFTER_MAX` / `BEFORE_BUILD` ("Sync rejected: PC clock?"), `ERR TIME FORMAT expected 'TIME <unix>[.ms]' or 'TIME YYYY-MM-DD HH:MM[:SS]'` ("Sync rejected: format") |
| `TIME 2026-10-11 19:42[:07]` | same, `q=MANUAL` | same |
| `TIME?` | `TIME utc=… local=… q=… src=… age=… zone=MSK` | — |

`delta` is device-minus-PC before the set: it is the measured drift.

**`tools/sync_time.py`** (Windows/Linux; `py -m pip install pyserial`):
1. Find the port: USB VID 0x303A (Espressif USB-Serial/JTAG), else the only COM port, else the argument.
2. Open with `dtr=False, rts=False` set **before** `open()`. A DTR/RTS sequence can reset the S3 over USB-Serial/JTAG.
3. `PING` every 300 ms for up to 10 s, until `PONG`. This also tolerates a reboot.
4. Wait for the next PC second boundary, then send `TIME <time.time():.3f>`.
5. Print the device reply and the drift. `--check` sends `TIME?` only and reports the offset (used by §11 step 16). `--tz MSK` also sends `TZ MSK`.
6. On failure, print: "No answer. Press any key on the gadget first (with the screen off it may be asleep and invisible to the PC for up to ~35 s, or powered off). Port busy? Close the Arduino Serial Monitor (Windows allows one program per COM port). Is 'USB CDC On Boot' Enabled?"

**`tools/sync/hg_sync.html`** (Chrome/Edge Web Serial; single file, no network):
- Connect button → `navigator.serial.requestPort({filters: [{usbVendorId: 0x303A}]})` → `open({baudRate: 115200})` → `setSignals({dataTerminalReady: false, requestToSend: false})`.
- Then the same PING loop and `TIME <Date.now()/1000>` aligned to a second boundary.
- Shows the reply, the drift, and a "Set zone = PC zone" button that maps `Intl.DateTimeFormat().resolvedOptions().timeZone` to an id.
- If opening the port resets the board, the PING loop absorbs it [HW?].
- If no port with VID 0x303A is offered, or PING gets no answer, the page says: "Press any key on the gadget first (screen off = asleep or powered off), then Connect again."

**Typed fallback** (no tools): in the Arduino Serial Monitor (line ending "Newline"), type `TIME 2026-10-11 19:42:00` and press Enter when a phone shows :00.

---

## 6. Drivers (register level)

### 6.1 I2C buses

| Bus | Pins | Clock | Timeout | Devices |
|---|---|---|---|---|
| `Wire` (bus 0, sensors) | SDA 8, SCL 7 | **100 kHz** (unknown pull-ups and wire lengths; tiny payloads) | `setTimeOut(20)` | SHT 0x44 (0x45), magnetometer 0x0D / 0x2C / 0x1E |
| `Wire1` (bus 1, display) | SDA 10, SCL 11 | 400 kHz (the SSD1306 spec maximum [HWF u8g2 U15]) | `setTimeOut(20)` | SSD1306 0x3C (0x3D fallback) |

**Return codes on core 3.3.12** [V-src `Wire.cpp`; HWF sht4x F21 VERIFIED]:
- `endTransmission()` maps `ESP_OK` → 0, `ESP_FAIL`/`ESP_ERR_NOT_FOUND` → 2, `ESP_ERR_TIMEOUT` → 5, anything else → 4.
- A zero-length probe NACK → `ESP_ERR_NOT_FOUND` → **2**.
- A NACK *with payload* surfaces as `ESP_ERR_INVALID_STATE` → **4**. A sync-path SCL timeout also gives 4.
- `requestFrom()` returns 0 on a NACKed read header ("busy" and "gone" look identical; use deadlines). `endTransmission(false)` does no I/O and returns 0.
- `II2cBus` therefore exposes only `Ok / Absent / Fail / Stuck`.

**Error handling:**
- Each driver counts consecutive failures. After 3, its health becomes Fault and it re-probes with back-off 5 → 10 → 30 → 60 s, only while the display is on.
- **Stuck-line check** before `begin()` and on 3 consecutive Fails:
  1. `pinMode(SDA, INPUT_PULLUP)`, `pinMode(SCL, INPUT_PULLUP)`, wait 50 µs, read.
  2. If either line is low → **recovery**: `Wire.end()`; SCL as `OUTPUT_OPEN_DRAIN` high; SDA `INPUT_PULLUP`; up to 9 SCL pulses (5 µs half period) while SDA is low; STOP (SDA low → SCL high → SDA high); re-check both lines high; `Wire.begin(sda, scl, f)` + `setTimeOut`.
  3. At most 3 recoveries per minute. If SDA stays low → `E-I2C-STUCK` (FATAL card §3.8.8); bus 0 is marked down until the next successful check (every 30 s).
- **Scan**: zero-length probe of 0x08..0x77. Runs at boot (logged), on Diagnostics page 2 and on `SCAN`. The result is reused in error texts ("bus saw 0x44").
- `I2cStats` count `ok/absent/fail/recoveries`, shown on Diagnostics page 1.

### 6.2 Temperature/humidity: SHT4x primary, SHT3x fallback

**Command set** [HWF sht4x F02/F03, VERIFIED]: every SHT4x command is one byte. The sensor does not clock-stretch; it NACKs the read header while busy. Data can be read once.

| Cmd | Function | Max time | Our wait |
|---|---|---|---|
| `0xFD` | measure T+RH, high repeatability | 8.3 ms | read at +10 ms, then retry every 3 ms until +30 ms |
| `0x89` | serial number (2 words + CRC each) | — (driver uses 10 ms) | read at +10 ms |
| `0x94` | soft reset (aborts a heater pulse) | 1 ms | 10 ms |
| `0x39` / `0x2F` / `0x1E` | heater 200 / 110 / 20 mW for 1 s, then a measurement | 1.1 s + 8.3 ms | poll the read every 5 ms from +1110 ms until +1200 ms |
| `0x32` / `0x24` / `0x15` | same, 0.1 s | — | not used |

- **CRC-8**: polynomial 0x31, init 0xFF, no reflection, no final XOR. Vectors: `BE EF → 92`, `66 66 → 93`, `80 00 → A2`, `00 00 → 81`, `FF FF → AC` (computed).
- **Conversion** [HWF F07, F15]: SHT4x `T = −45 + 175·S/65535`, `RH = −6 + 125·S/65535` clamped to 0..100. SHT3x RH = `100·S/65535` (no clamp needed). Vectors: `0x6666` → 25.00 °C; `0x8000` → 56.50 %RH (SHT4x) / 50.00 %RH (SHT3x).
- **User temperature offset**: `t = tRaw + offset`; `rh = correctRhForTemp(rhRaw, tRaw, t)` keeps the dew point constant (C). Vector: 50 % at 25 °C corrected to 23 °C → 56.38 %.
- **Derived**: dew point (Magnus 17.62/243.12): 25 °C/50 % → 13.85 °C; 23.4/56 → 14.12 °C. Absolute humidity `216.7·(RH/100·6.112·e^{17.62T/(243.12+T)})/(273.15+T)`: 23.4/56 → 11.75 g/m³.

**Probe** (non-blocking `beginProbe`/`tick`), per address 0x44 then 0x45. **SHT4x is always ruled out before any SHT3x command** [HWF F18/F19]: the SHT3x single-shot command byte `0x24` is the SHT4x 110 mW heater command.

```
hint.busyWaitUs > 0 → wait that long first (≤ 1.2 s)      (RtcState.shtBusyUntil: the MCU reset during a heater pulse)
hint.cached ∈ {Sht4x, Sht3x} (DEEPSLEEP wake only) → one check at hint.addr with CRC: Sht4x 0xFD measurement (10 ms);
   Sht3x status read 0xF3 0x2D — never 0x2400 here, a sensor swapped during sleep could be an SHT4x [HWF F18/F19]
   ok → done, chip = cached (≈ 12 ms)    fail → full probe below
probe(A) NACK → next address; both NACK → None: E-SHT-NONE "No temp sensor / Check SDA8 SCL7 3V3"
write 0x94; wait 10 ms                                   (aborts a heater pulse left over from a warm reset, if accepted while busy)
write 0x89; wait 10 ms; read 6
   both CRCs ok → Sht4x, serial = w0<<16 | w1
   read NACK → one retry: write 0x89, wait 10 ms, read 6 (≤ 20 ms extra)
else wait 40 ms; write 0xF3 0x2D (status); wait 2 ms; read 3; CRC ok → Sht3x; write 0x3041 (clear status)
else → Unknown at A: E-SHT-UNK "Unknown chip at 0x44 / Not SHT4x / SHT3x"
```
Never send a general-call reset (it would hit the magnetometer) [HWF sht4x pitfalls].

Probe time (D33): an SHT3x is now identified ≈ 90 ms after the address ACK instead of after a 1.2 s busy-retry, so warm boots keep their ≈ 0.3 s target and the 1.5 s timer-wake lead is not eaten. The 1.2 s wait remains only where it is needed: `ThermoService` writes `RtcState.shtBusyUntil = rtcNow + 1.2 s` when a heater command is ACKed, and the probe waits out the remainder after a warm reset. Whether a busy SHT4x accepts 0x94 at all is undocumented [HWF sht4x notes]; if it does not and `RtcState` is invalid, the probe may report `Unknown` once, and the normal 5 s re-probe finds the SHT4x. `AppCore` stores the detected chip and address in `RtcState.shtChip/shtAddr`.

**Runtime** (driver state machine, all waits are `MonoUs` deadlines; the bus stays free for the magnetometer while the SHT works):
```
IDLE ─startMeasure→ CMD(0xFD | 0x2400 for SHT3x) → WAIT(10 | 16 ms) → READ 6 (CRC both) → Sample
IDLE ─startHeater→ CMD(0x39|0x2F|0x1E) → WAIT 1110 ms → READ (poll 5 ms, deadline +90 ms) → Sample{hot=true}
error (Fail on write, read still NACK at deadline, CRC) → Error; ThermoService counts; 3 in a row → Fault + back-off re-probe
```

**Heater: what the "Heater" setting means, and what it costs:**
- *Mechanism* [HWF F08]: a heater command heats for 1 s, then measures, then switches off by itself. There is no off command and nothing stays on if the MCU crashes.
- *Purpose* [HWF F09]: remove condensation or spray (RH stuck near 100 %), and reduce **creep** (a reversible positive RH offset after long exposure above ~90 %RH).
- *Our setting*: `Off` (default), `Auto`, `Every 5/10/30/60 min`. Each pulse is one 1 s pulse at the chosen power (default Mid 110 mW).
  - `Auto`: pulse only when RH ≥ 90 % on two consecutive readings, at most every 10 min (useful in fog, dew, tents). While dark, Auto reads every 60 s to evaluate RH.
  - Honest note in the picker sub-label and README: Sensirion's validated creep scheme is 200 mW/1 s every minute; a single pulse every 5–60 min is a **product choice whose effect is not validated** [HWF sht4x "missing facts"].
- *Accuracy*: right after a pulse the die is tens of °C above ambient (+31 °C typical at 110 mW on Sensirion's PCB [HWF F10]).
  - The hot sample is never published; it is logged (`I SHT  heat pre=23.4 hot=54.1 d=+30.7`) and kept for Diagnostics.
  - **Cool-down**: publishing is suspended for at least 20 s. While the display is on, it resumes when |T − T_pre| ≤ 0.3 °C or |dT/dt| < 0.05 °C/s over 10 s, with a cap of 90 s [HW?: tuned in §11 step 8]. While dark, it simply waits 90 s.
  - During cool-down Home shows the frozen pre-pulse values with the flame glyph.
  - Cool-down starts when the heater command is **ACKed**, not when the hot sample is read. A lost or NACKed hot sample (sensor reset by supply sag) must never let the next normal reading publish hot values [HWF sht4x verifier notes].
- *Safety* [HWF F08]: maximum duty 10 %, ambient < 65 °C, supply sag can reset the sensor. `heaterDue()` enforces:
  - ≥ 60 s between pulses → worst case 1 s per 5 min = 0.33 % duty;
  - no pulse if the last valid T > 60 °C → `SkipHot`, logged, toast once "Heater skipped: >60°C", Details row;
  - no pulse while a measurement is in flight;
  - no pulse while a deep sleep is pending (`HeaterIn.shutdownPending` → `SkipShutdown`; set from the moment power-off is requested until the sequencer ends) — the sequencer itself waits for a pulse already running, ≤ 1.2 s.
- *SHT3x*: its heater is continuous and meant only for plausibility checks [HWF sht4x missing facts]. The Heater row shows `N/A` with sub-label `SHT3x has no pulse heater` and is not editable. An info card is shown once per detected chip change (`I-SHT-3X`).

### 6.3 Magnetometer autodetect and drivers

Probe order `0x0D → 0x2C → 0x1E` with a zero-length probe each. The chip is **identified by its ID register, not by its address**: the listing says QMC5883L, the silkscreen says HMC5883L, and P variants are also sold as GY-273 [HWF qmc PROBE].

| Chip | Addr | Identity | Data | Status | Scale used |
|---|---|---|---|---|---|
| QMC5883L | 0x0D | reg 0x0D == 0xFF. This is a **weak** check (an early datasheet had it reserved): a mismatch → treat as QMC5883L with `W-MAG-ID` (on-screen warning in Diagnostics, not a refusal) [HWF Q14] | 0x00..0x05 X,Y,Z int16 **LE** | 0x06: b0 DRDY, b1 OVL, b2 DOR | RNG 8 G → 3000 LSB/G (30 LSB/µT) |
| QMC5883P | 0x2C | reg 0x00 == 0x80 → "QMC5883P-compatible" (ID shared with QMC6308/6310 [HWF ID-SHARED]) | 0x01..0x06 X,Y,Z int16 LE | 0x09: b0 DRDY (cleared by reading 0x09), b1 OVFL | RNG 8 G → 3750 LSB/G (37.5 LSB/µT) |
| HMC5883L | 0x1E | regs 0x0A..0x0C == 'H','4','3' | 0x03..0x08 **X, Z, Y** int16 **BE**; −4096 = overflow | 0x09: b0 RDY, b1 LOCK | ±1.3 Ga → 1090 LSB/Ga (10.9 LSB/µT) |
| anything else ACKing at those addresses | — | → `E-MAG-UNK`, screen shows address and ID byte | — | — | — |

**QMC5883L** [HWF Q8–Q24]:
- `start()`: `0x0A = 0x80` (soft reset → standby, defaults); after 10 ms: `0x0B = 0x01` (SET/RESET period), `0x0A = 0x00` (**INT_ENB = 0 → DRDY pin enabled**, ROL_PNT = 0; v1 wrote 0x01, which *disables* DRDY [HWF Q12]), `0x09 = 0x15` (OSR 512, RNG 8 G, ODR 50 Hz, continuous). Read back 0x09 and 0x0B; a mismatch is an init failure.
- `standby()`: `0x09 = ctrl1 & ~0x03` (= 0x14, 3 µA) [HWF Q19]. Resume = rewrite 0x09.
- Read: status 0x06 **first** (1 byte); if DRDY → burst 0x00..0x05 in one transaction (a partial read locks the data registers [HWF Q17]). OVL → sample marked overflow (not used for heading or calibration).

**QMC5883P** (values from the QST datasheet Rev E, **not** from third-party drivers, which carry wrong constants [HWF THIRDPARTY-BUGS]):
- `start()`: `0x0B = 0x80` (soft reset); after 5 ms: `0x0B = 0x00` (in case SOFT_RST does not self-clear), `0x29 = 0x06` (axis sign, datasheet setup step; behind `kQmc5883pWriteSignReg = true`), `0x0B = 0x08` (set/reset on, RNG 8 G), `0x0A = 0x00` (suspend; mode changes must pass through suspend [HWF P-STANDBY]), then `0x0A = 0xC5` (OSR2 = 8, OSR1 = 8, ODR 50 Hz, **Normal** mode, about 310 µA). Continuous mode draws about 2.2 mA and is not used [HWF P-INIT-CONT].
- `standby()`: `0x0A = 0x00` (suspend, 22 µA).
- Read: status 0x09 once (it clears DRDY and keeps OVFL), then burst 0x01..0x06.
- **No DRDY pin** on this chip [HWF P-NO-DRDY-PIN]: GPIO12 is configured `INPUT_PULLDOWN` and never used.
- **Axis rotation**: on the same footprint the P is rotated 90° from L/HMC (`L_X = P_Y, L_Y = −P_X` [HWF P-AXES]). The driver converts to the board frame: `bx = py, by = −px, bz = pz` [HW?].

**HMC5883L** [HWF hmc F06–F15]:
- `start()`: `0x00 = 0x70` (CRA: 8 averages, 15 Hz, normal), `0x01 = 0x20` (CRB: ±1.3 Ga), `0x02 = 0x00` (continuous). Discard the samples of the first 2/f = 133 ms and one sample after any CRB write.
- `standby()`: `0x02 = 0x02` (idle, 2 µA). Always burst-read all 6 bytes from 0x03. **Never read MODE in the loop** (sets LOCK [HWF F13]).
- **Dead status register ("DA 5883", D34)** [HWF Q32]: an undocumented chip answers at 0x1E with the HMC map and the `H43` ID, so it passes identification, but its status register is broken. Rule for every chip at 0x1E: if RDY stays 0 for `kMagNoStatusMs` = 500 ms after `start()` (counted after the 133 ms discard), read the data registers 3 times one ODR period (67 ms) apart. If the values change → switch to **timed reads** (burst 0x03..0x08 every 1/ODR = 67 ms, no status check), set `noStatus()`, raise `W-MAG-NOSTATUS` (Diagnostics `MAG HMC5883L 1E NO STATUS`, log). If they do not change → `E-MAG-STUCK` as usual. In timed mode the stuck rule uses "25 identical consecutive samples" only.

**Polling** (no interrupts in the data path):
- Every `kMagPollMs = 20` ms while demanded: read status; if ready, read data.
- Discard the first 2 samples after `start()`.
- **Stuck detection**: no new sample for 500 ms, or 25 identical consecutive samples → `E-MAG-STUCK`, re-run `start()` (at most once per 5 s). Exception: a chip at 0x1E whose RDY never sets is first tested for the dead-status case above, so it does not restart every 5 s forever.
- Overflow > 10/s → `W-MAG-SAT` (tag `SATURATED`).

**DRDY on GPIO12**: diagnostics only, attached while Diagnostics page 1 is open.
- QMC5883L: `INPUT` (push-pull, active-high level), count RISING edges.
- HMC5883L: `INPUT`, count FALLING edges (active-low 250 µs pulse) [HWF hmc F12].
- QMC5883P: no pin → `INPUT_PULLDOWN`, shows `NO PIN (QMC5883P)`.
- A RISING-only ISR can stall on a level-held QMC line; this is harmless because nothing depends on it.

### 6.4 Heading math and calibration

Pipeline: board-frame raw `(bx, by, bz)` → calibration (sensor frame) → mount → filter → heading → declination → display.

1. **Calibration (2-D hard + axis-aligned soft iron)**: `xc = (bx − offX)·scaleX`, `yc = (by − offY)·scaleY`. A mount change does not invalidate the calibration (same sensor axes).
2. **Mount** (`rot` = angle of the silk X arrow clockwise from the device's forward edge, i.e. the screen's top edge; `chipDown` = board mounted upside-down):
   `if (chipDown) yc = −yc;  F = xc·cosθ + yc·sinθ;  L = −xc·sinθ + yc·cosθ`.
   Sensor convention: X forward, Y **left**, Z up when the chip faces up [HWF hmc F18].
3. **Filter**: `VectorEma` on (F, L) with τ by needle setting (Calm 0.25 s, Normal 0.12 s, Fast 0.06 s), `α = 1 − e^{−dt/τ}`. Filtering the vector, not the angle, avoids wrap artefacts [HWF hmc F24].
4. **Magnetic heading** `ψm = atan2(L̄, F̄)` → [0, 360). Facing north → field along +F → 0°. Facing east → north is to the left → 90°.
5. **True heading** `ψ = wrap360(ψm + declination)`; east is positive [HWF hmc F20]. The status line shows `True +12` when the declination ≠ 0, otherwise `Mag`.
6. **Display**: digits `shownHeading()` with ±0.75° hysteresis; the card follows a `Spring` toward ψ (§3.6); 16-point name `point16(ψ)` = sector `⌊(ψ + 11.25)/22.5⌋ mod 16`; target steer `Δ = wrap180(target − ψ)` (positive = turn right).
7. **Health tags**: `SATURATED` on overflow. When calibrated: `r = hypot(F̄, L̄)`; `r/rCal ∉ [0.6, 1.6]` for > 1 s → `FIELD?` + "Metal near?" (heading still shown). Uncalibrated → `UNCAL`.

**Calibration session** (`MagCalibrator`; level only, two slow turns):
- Samples arrive at ≤ 50 Hz and are decimated into a ring of 360 `(x, y, z)`. A median-of-3 per axis is taken before min/max. Overflow samples are rejected (hint `Metal near?`).
- **Level check** (physics: at inclination ≈ 72° the vertical field is about 3× the horizontal [HWF hmc F17]; tilting injects it into X/Y extremes):
  - `zRef` = median z of the first 2 s.
  - A sample with `|z − zRef| > 0.15·r̄` (≈ 9° of tilt) is rejected.
  - If more than 20 % of the last 50 samples were rejected → hint `Keep level!`.
- Angular rate > 90°/s (estimated from consecutive bins) → hint `Turn slower`.
- `offX = (maxX + minX)/2`, `rX = (maxX − minX)/2` (same for Y), `r̄ = (rX + rY)/2`, `scaleX = r̄/rX`, `scaleY = r̄/rY`.
- **Coverage**: 24 bins of 15° by `atan2(y − offY, x − offX)` using the running estimate. A bin counts after ≥ 2 accepted samples with radius ≥ 0.5·r̄.
- **Grades** (`q = 1 − σ(r)/mean(r)` over corrected samples):

  | Grade | Coverage | Axis ratio rY/rX | q |
  |---|---|---|---|
  | GOOD | ≥ 95 % (23/24) | 0.85–1.15 | ≥ 0.92 |
  | OK | ≥ 90 % (22/24) | 0.70–1.40 | ≥ 0.85 |
  | POOR | otherwise: not savable, reason shown | | |

- **Plausibility**: horizontal field `r̄ / lsbPerUt` must be in 5..60 µT (Moscow ≈ 16.6 µT [HWF hmc F17]), else POOR (`Field weak` / `Metal near?`).
- Minimum 8 s; timeout 120 s → FAILED with the reason.
- **Saved blob** NVS `cal` = `{u32 magic 'HGC1', u8 ver 1, u8 chip, f32 offX, offY, scaleX, scaleY, rCal, lsbPerUt, u8 quality, i64 savedUtcS, u32 crc32}`. If the detected chip ≠ `cal.chip` → the calibration is ignored, `W-MAG-CALCHIP`, `UNCAL`.
- Runtime heading error from tilt is about 3° per 1° [HWF hmc F23]; do not present the heading as better than about ±5°.

### 6.5 Battery (stub, one config line to enable)

- `kPinBatteryAdc = -1` → `IBattery::read()` returns `BatteryState{present = false}` (a `core/types.h` type, so `hal` never depends on `ui`); Home shows the dashed glyph; the boot report and `INFO` say `bat not fitted`. Details does not list the battery (one rule, §3.8.1).
- To enable: set `kPinBatteryAdc = 2` (GPIO2 = ADC1_CH1, not a strapping pin; avoid GPIO3, which is a strapping pin and may carry a VBUS/2 divider [HWF SM-14/SM-15]) and `kBatteryDividerRatio`.
- Reading: `analogReadMilliVolts` [V-build] averaged over 16 samples every 30 s, then the LiPo table {4200:100, 4100:90, 4000:80, 3900:65, 3800:50, 3700:35, 3600:20, 3500:10, 3400:5, 3300:0} with ±2 % hysteresis. Warn below 3.45 V (`W-BAT-LOW`, v2.1).
- Note: on battery the 3V3 rail drops out once VBAT < ~3.55–3.8 V (LDO dropout + Schottky) [HWF supermini pitfalls], so "0 %" must be calibrated on hardware.

### 6.6 Display HAL (`hal/hal_oled.cpp`)

U8g2 C API with **our own byte callback on Wire1**. Never `U8G2_*_2ND_HW_I2C`: it compiles to a silent no-op on arduino-esp32 [HWF u8g2 U2/U3]. Compiled in this session [V-build]:

```cpp
static uint8_t s_lastErr; static uint32_t s_ioErrors; static uint8_t s_consecErr;
static uint8_t hg_byte_wire1_cb(u8x8_t* u, uint8_t msg, uint8_t n, void* p) {
  switch (msg) {
    case U8X8_MSG_BYTE_INIT: case U8X8_MSG_BYTE_SET_DC: return 1;           // Wire1 is begun by the app, never here
    case U8X8_MSG_BYTE_START_TRANSFER: Wire1.beginTransmission(u8x8_GetI2CAddress(u) >> 1); return 1;
    case U8X8_MSG_BYTE_SEND: Wire1.write((const uint8_t*)p, n); return 1;   // ≤ 25 bytes per transfer
    case U8X8_MSG_BYTE_END_TRANSFER:
      s_lastErr = Wire1.endTransmission();
      if (s_lastErr) { ++s_ioErrors; ++s_consecErr; } else s_consecErr = 0;
      return 1;
  }
  return 0;
}
static bool s_fastInit;   // true on warm wakes: the panel stayed powered
static uint8_t hg_gpio_delay_cb(u8x8_t*, uint8_t msg, uint8_t n, void*) {
  if (msg == U8X8_MSG_DELAY_MILLI && !s_fastInit) delay(n);   // skips the 3×100 ms reset helper delays when warm [HWF U10]
  return 1;
}
```
- **Init**: `Wire1.begin(10, 11, 400000)` **before** u8g2 init [HWF U6]. Probe 0x3C, then 0x3D (zero-length writes). Then `oledSetup()` (`ui/oled_setup.cpp`, host-tested) in exactly this order:
  1. `u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, hg_byte_wire1_cb, hg_gpio_delay_cb)` (or `_sh1106_` when `kOledController == Sh1106`);
  2. `u8x8_SetI2CAddress(&u8g2.u8x8, addr << 1)` — **after** setup: `u8g2_SetupDisplay` → `u8x8_SetupDefaults` resets `i2c_address` to 255 (`u8x8_setup.c:106`) and `CAD_INIT` then turns 255 into 0x78 (`u8x8_cad.c:566`), so an address set before setup is lost and a 0x3D panel would be driven at 0x3C [V-src];
  3. `u8g2_InitDisplay`; 4. `u8g2_SetFlipMode(&u8g2, kOledFlip)`; 5. `u8g2_SetPowerSave(0)`; then the brightness triple.
  
  `test_oled_setup`: with a recording byte callback, `oledSetup(…, addr7 = 0x3D, …)` must emit every transfer to 7-bit address 0x3D. `s_fastInit = (reset reason is DEEPSLEEP or SW)`, behind `kOledFastWake = true` [HW?].
- **Commands** via `u8x8_cad_StartTransfer / u8x8_cad_SendCmd / u8x8_cad_EndTransfer` [V-build]:
  - contrast `0x81 v`, precharge `0xD9 v`, VCOMH `0xDB v`;
  - invert `0xA7` / normal `0xA6`;
  - on `0xAF` / off `0xAE`;
  - `deepOff()` = `0xAE, 0x8D, 0x10` (charge pump off).
  - After a deep-sleep wake, the u8g2 init sends `0x8D 0x14` again.
- **Flush**: `sendPageSpan(page, tx0, tw)` = `u8g2_UpdateDisplayArea(&u8g2, tx0, page, tw, 1)` [V-build].
- **Health**: no ACK at both addresses, or 3 consecutive transfer errors → `ok() = false`, `E-OLED-NONE` / `E-OLED-IO`. `retryInit()` runs every 10 s and first sends only a zero-length probe to 0x3C/0x3D (≈ 0.1 ms each); only if one ACKs does it run `oledSetup()` + init, with `s_fastInit = true` (no 3 × 100 ms reset-helper delays: the panel is already powered and answering), so a retry never blocks the loop for ~300 ms. Serial prints the banner every 10 s. The device keeps running headless.
- `kOledFlip` (bool, default `false`): `true` if the module is mounted upside-down (§11 step 2). It uses the controller's hardware remap (`u8g2_SetFlipMode` → `0xA0`/`0xC0` instead of `0xA1`/`0xC8` [V-src `u8x8_d_ssd1306_128x64_noname.c` 222–236]), never `U8G2_R2`: R2 rotates the drawing inside the buffer, which would reverse the slide compositor's direction and move the rail with the content. The buffer layout (LSB = top row, rail at x 119..127) stays logical either way.

### 6.7 How faults reach the human (a silent program is a defect)

| Condition | On screen | Serial |
|---|---|---|
| SHT missing / unknown / repeated errors | Boot card; Home climate row text; ⚠ chip; Details row | `E SHT  E-SHT-NONE no ACK at 0x44/0x45 (bus saw: 0D)` |
| SHT3x instead of SHT4x | Info card once per chip change; Heater row `N/A` | `I SHT  SHT3x at 0x44 (heater unsupported)` |
| Heater skipped (hot / spacing) | Details `Heater: Skipped: >60°C`; toast once | `W SHT  heater skipped: T=61.2>60` |
| Magnetometer missing / unknown / stuck | Boot card; Compass card with addresses and ID | `E MAG  …` |
| QMC5883L ID ≠ 0xFF | Diagnostics `ID 12 (!)` | `W MAG  W-MAG-ID id=0x12, assuming QMC5883L` |
| Not calibrated / calibration for another chip | `UNCAL` tag + `CAL` label | `W MAG …` |
| Interference / saturation | `FIELD?` / `SATURATED` tag | `W MAG …` |
| I2C bus stuck | FATAL card (§3.8.8) | `E I2C  E-I2C-STUCK SDA8 low after 9 clocks` |
| OLED missing | — (impossible) | banner every 10 s |
| Settings value invalid / NVS failure | Card `SETTINGS RESET` naming the key / FAULT card `NVS FAILED` (settings RAM-only) | `W CFG  'scr'=17 invalid → 30` |
| A picker save whose NVS write fails | Toast `Not stored: storage` (value kept for this session); `NVS FAILED` card once per boot | `E NVS  put 'scr' failed` every time |
| Clock lost after power-off | `SYNC?` + toast | hint line |
| Sync rejected / bad command | Toast with the reason | `ERR …` |
| Key held LOW 10 s while awake (D26) | KEY STUCK card **now**; key ignored; toast `Key # works again` on recovery | `W KEY  K3 (GPIO6) held 10 s, ignored` / `I KEY  K3 released` |
| Stuck key at sleep, phantom wake storm | KEY STUCK card at next boot | `W KEY  K3 stuck, excluded from wake` / `W KEY  K3 5 phantom wakes in 2 min, excluded` |
| Deep sleep rejected | Toast `Sleep failed`, stays awake; `E-PWR-SLEEP` in Diagnostics | `E PWR  E-PWR-SLEEP …` |
| Manual power-off while the timer ends within lead + 1 s | Toast `Off after timer alert`; powers off after the alert is dismissed or ends | `I PWR  power-off deferred until the alert` |
| `+1 min` at the 10 h cap / lap 1000 | Toast `Max 10 h` / `Max 999 laps` | `I TMR …` / `I SW  …` |
| Restored after crash | Toast `Restored after crash` | `W BOOT reset=TASK_WDT restored` |
| Deep-sleep state lost (CRC/layout) or crash loop | STATE LOST / TIMER LOST card, display on | `E BOOT E-RTC-LOST reset=DEEPSLEEP rtc_state invalid` |
| 0x1E magnetometer with a dead status register | Diagnostics `NO STATUS`; works with timed reads | `W MAG  W-MAG-NOSTATUS RDY stuck 0, timed reads` |
| Serial output dropped (host stopped reading) | Diagnostics › System counter | `[N bytes dropped]` line once output resumes |
| A draw refused by `Canvas` (layout overflow that escaped the tests) | — | `W UI   <screen>: N draws rejected` once per screen per boot |
| USB CDC On Boot disabled, or USB Mode ≠ Hardware CDC | — | **compile-time `#error`** in `hal/build_checks.h` naming the IDE menu items (D35). A `#warning` would be invisible: the IDE's default "Compiler warnings: None" passes `-w`, which suppresses `#warning` [V-src platform.txt 39–40; V-meas g++], and with CDC off `Serial` is `Serial0` (`HardwareSerial.h` 441–452), which has no `setTxTimeoutMs()`/`isConnected()`, so the build would otherwise fail with a cryptic member error. |

---

## 7. Power

### 7.1 States

| State | Display | CPU | Sensors | What keeps running | Left by |
|---|---|---|---|---|---|
| **ACTIVE** | on, chosen brightness | 80 MHz; `delay(≤ 5 ms)` between iterations | per demand | everything | timeout − 5 s → DIM |
| **DIM** | on, Low triple (§3.7) | same | same | same | key (swallowed) → ACTIVE; timeout → OFF |
| **OFF** (display off) | `0xAE` | **light sleep** between deadlines when allowed (§7.2), else idle loop | magnetometer standby; SHT idle (Auto heater: 60 s reads; scheduled pulses) | models, alert deadline, clock, heater schedule | key (GPIO wake, swallowed) → ACTIVE; timer done → ACTIVE + alert; auto power-off → DEEP |
| **DEEP** (power off) | `0xAE` + charge pump off | deep sleep | standby | RTC timer (wall clock and `RtcUs`), `RTC_NOINIT` snapshot, ext1 logic, RTC pull-ups | ext1 key → boot (swallowed); RTC timer (timer deadline) → boot + alert |

Transitions:
- Screen timeout (§2.3).
- **Auto power-off** after `AutoOff` (default 30 min) in OFF. Not while a USB host is present, an alert is active, `keepAwake` holds, or the timer ends within 60 s (the device just light-sleeps until the deadline).
- **Manual**: hold `*` on Home 1.5 s, or Settings › Power off (hold `*` 1.5 s). If the countdown runs and ends within `kTimerWakeLeadMs` + 1 s (too soon to arm the RTC wake, §7.5 step 5), the power-off is **deferred, not silently downgraded**: toast "Off after timer alert", the display turns off now, and the shutdown sequence runs when the alert is dismissed or phase B ends.
- A running stopwatch or timer does **not** block deep sleep: they persist (§5.4), and a running timer arms the RTC wake.

### 7.2 `decidePower()` rules (pure, host-tested)

- **display**:
  - alertActive or keepAwake → On;
  - keyDown (a non-stuck key is held) → current state kept, timeout suspended (§2.3.3); a stuck key does not count;
  - screenTimeout == 0 → On;
  - idle ≥ timeout → Off;
  - idle ≥ timeout − dimLead → Dim (dimLead 5 s, or 3 s when timeout = 10 s);
  - else On.
- **lightSleep** iff all hold:
  - display == Off;
  - LightSleep setting = On;
  - `!usbHost` (debounced);
  - `!usbRaw` and `!usbPending`: while the raw `isPlugged()` signal is true, or a debounce is counting, the loop stays awake so the 1 s debounce can complete (D28);
  - `now ≥ usbProbeUntil` (USB probe window, below);
  - no **non-stuck** key down (`keyDown` excludes stuck keys, §2.2);
  - `!sensorBusy` (no I2C op in flight; SHT heater wait is allowed);
  - not animating;
  - `nextDeadline − now ≥ kLightSleepMinMs = 50`.

  Then `lightSleepUs = min(nextDeadline − now, kLightSleepMaxSliceMs = 4000) · 1000`. The 4 s slice keeps the loop WDT fed whatever TWDT does in light sleep [HW?]; the cost is about 9 ms awake per slice [HWF F20], ≈ 45 µA average [EST].
- **USB probe window** (D28). Light sleep gates the USB-Serial/JTAG pads [HWF F26]. A host needs ≈ 100 ms of attach debounce plus a bus reset before the first SOF, so after a ≤ 4 s slice the raw signal is still false when the loop sleeps again within milliseconds: a PC plugged in while the screen is off would never be seen (it would show the device appearing and vanishing). So `PowerManager` sets `usbProbeUntil = now + kUsbProbeAwakeMs (300)` after every `kUsbProbeEverySlices` = 8th light-sleep wake (≈ every 32 s); a key wake turns the display on and keeps the loop awake anyway. If the host's SOFs arrive inside the window, `usbRaw` goes true, light sleep stays off, and after 1 s the debounced `usbHost` blocks light sleep and auto-off. Cost ≈ 300 ms × 25 mA per 32 s ≈ +0.25 mA while dark [EST]. Unverified on hardware [HW?] (§11 step 23).
- **Stuck keys** are excluded from the GPIO wake set (`IPower::lightSleep(us, input.stuckMask())`), otherwise their LOW level would end every light sleep at once.
- **autoPowerOff**: display Off ∧ autoOffMs ∧ `displayOffSince + autoOffMs ≤ now` ∧ !usbHost ∧ !alertActive ∧ !keepAwake ∧ !(timerRunning ∧ remaining < 60 s).
- `nextDeadline` = earliest of: timer deadline, alert phase change, SHT due (only scheduled work while dark), heater due, clock checkpoint, auto-off moment, USB debounce re-check (1 s while plugged), end of the USB probe window, DIM/OFF moments.

### 7.3 USB-CDC caveats

- `Serial` is HWCDC (USB-Serial/JTAG) with `CDCOnBoot=cdc` [HWF F03].
- **Light sleep gates the USJ pad and clock**: the host sees a disconnect or stall and may need a re-plug [HWF F26]. So: no light sleep while a USB host is present.
- **Host detection**: `HWCDC::isPlugged()` = `usb_serial_jtag_is_connected()` (SOF watchdog; any host or hub; false on a charger) [V-src HWCDC.h]. The core's own comment says it "is known to flap even on a healthy link" [V-src HWCDC.cpp]. `UsbDebouncer`: plugged after 1 s continuously true, unplugged after 3 s continuously false. It starts as plugged (the raw signal starts true at boot [HWF F28]).
- `Serial.setTxBufferSize(4096)` **before** `begin()`, then `setTxTimeoutMs(0)`: writes never block. When not connected, output goes into the drop-oldest ring [V-src HWCDC.cpp `flushTXBuffer`]. When connected, `HWCDC::write()` with timeout 0 gives up once the ring is full and returns a **short count** (20 failed `xRingbufferSend(…, 0)` attempts, then `size = so_far`) [V-src HWCDC.cpp 540–600]. With the default 256-byte ring the boot report (~1 KB), `INFO`, `HELP`, `LOG DUMP` (48 × 96 B ≈ 4.6 KB) and the replay were cut to about 256 B without a trace (D31).
- **Paced output** (`app/logger`): `ISerialPort::write()` returns the accepted count and `writable()` returns `availableForWrite()`. The `Logger` keeps a 1 KB FIFO for live lines and a **cursor** for long outputs (boot report, `INFO`, `HELP`, `LOG DUMP`, replay), which are generated line by line from their source (`LogRing`, current state) instead of being copied. Once per loop iteration (`logger.pump()`, §4.5 step 4) it writes as much as `writable()` allows, at most 1 KB per iteration. Live lines that do not fit into the FIFO are dropped and counted; when output resumes the `Logger` first prints `[N bytes dropped]`. Nothing is truncated silently; the counter is also in Diagnostics › System (`SystemView.serialDropped`). `test_logger` (FakeSerial with a small ring) checks pacing, ordering and the drop line.
- **Flush before sleep**: with `tx_timeout_ms == 0`, `HWCDC::flush()` sets `tries = 0`, skips the drain loop, marks the link disconnected and **discards** the queued TX bytes [V-src HWCDC.cpp lines 629–660]. `ISerialPort::flushBeforeSleep()` therefore does: if `Serial.isConnected()` → `setTxTimeoutMs(50); flush(); setTxTimeoutMs(0)`; otherwise skip. Bounded: at most 50 ms with no progress.
- Log ring dump: when `Serial.isConnected()` goes false → true, the boot report and the last 48 log lines are replayed (the monitor attached after boot still sees everything).
- Never `while (!Serial)`: `operator bool` is false on battery forever [HWF F29].
- Deep sleep drops USB by design [HWF F27]. If an upload or a sync fails because the gadget is asleep or powered off, press a key first, or use BOOT + RESET (§10.6). With the screen off (light sleep) a PC is noticed within ≈ 35 s by the USB probe window (§7.2); a key press makes it immediate.

### 7.4 Light-sleep sequence (`hal_power.cpp::lightSleep(us, excludeKeys)`) [V-build]

```cpp
// wakePins = keyPins minus the stuck keys in excludeKeys (D26)
for (pin : wakePins) if (gpio_get_level(pin) == 0) return SleepResult::Rejected;   // level wake would fire at once
esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);   // wake sources persist across sleeps otherwise [HWF F19]
for (pin : wakePins) gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);   // gpio_sleep_sel_dis(pin) was done once at boot
esp_sleep_enable_gpio_wakeup();
esp_sleep_enable_timer_wakeup(us);                       // us ≥ 50 000
esp_err_t r = esp_light_sleep_start();
for (pin : wakePins) { gpio_wakeup_disable(pin); gpio_set_intr_type(pin, GPIO_INTR_DISABLE); }   // restore the type [HWF F18]
return r == ESP_OK ? Ok : r == ESP_ERR_SLEEP_REJECT ? Rejected : r == ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION ? TooShort : Error;
```
- `esp_timer`/`millis()` are compensated; the FreeRTOS tick count is **not** [HWF F23], so nothing may use `xTaskGetTickCount` for time.
- After a GPIO wake the S3 cannot report which pin woke it [HWF F17]. The next `feed()` reads the levels, and the gate swallows that key.
- Rejected/TooShort are counted (Diagnostics) and treated as an immediate wake.

### 7.5 Deep-sleep sequence (shutdown sequencer in `PowerManager` + `hal_power.cpp::deepSleep`)

Sequencer states, non-blocking (pure, host-tested):
1. **Persist**: set `HeaterIn.shutdownPending` (no new heater pulse from now on); write the NVS `clk` checkpoint; seal `RtcState` (`lastSleep` = auto/manual).
2. **Card** (manual only): POWER OFF card 1 s (§3.8.8).
3. **Quiesce sensors**: `mag.standby()`; wait for any SHT measurement or heater pulse to finish (≤ 1.2 s).
4. **Keys**: keys already stuck at runtime (§2.2) are removed from the wake mask at once; then wait until the other keys are released (≤ 3 s). Pins still LOW are **removed from the wake mask**; `stuckKeyMask` is stored; a KEY STUCK card shows at the next boot. If all four are stuck → abort deep sleep, show the card now, stay in OFF.
5. **Timer**: if the countdown runs and `remaining > kTimerWakeLeadMs + 1 s` → `plan.timerUs = remaining − kTimerWakeLeadMs` (lead 1500 ms, covers boot). Otherwise do not deep-sleep: a manual power-off is deferred until after the alert with the toast "Off after timer alert" (§7.1); auto power-off is already vetoed in that case (§7.2).
6. **Directive Deep(plan)** → `hal_power.cpp::deepSleep(plan)` [V-build]:

```cpp
display.deepOff();                                   // 0xAE, 0x8D 0x10
Wire.end(); Wire1.end();
esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
esp_sleep_disable_ext1_wakeup_io(0);                 // clear any previous ext1 pin set [HWF F06]
for (pin : plan.ext1Mask) { rtc_gpio_pullup_en(pin); rtc_gpio_pulldown_dis(pin); }   // RTC pulls ≠ digital pulls on S3 [HWF F09]
if (plan.keepRtcPeriph) esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);  // ≈ +1 µA [HWF F12]
esp_sleep_enable_ext1_wakeup_io(plan.ext1Mask, ESP_EXT1_WAKEUP_ANY_LOW);   // mask 0x72 = GPIO1|4|5|6 minus stuck keys
if (plan.timerUs) esp_sleep_enable_timer_wakeup(plan.timerUs);
serial.flushBeforeSleep();                           // §7.3
esp_err_t r = esp_deep_sleep_try_to_start();         // returns only on rejection
// → re-init buses and display, return SleepResult::Rejected; AppCore toasts "Sleep failed", logs E-PWR-SLEEP, stays awake
```

**On wake** (§4.6 step 2):
- For every key pin: `rtc_gpio_hold_dis` + `rtc_gpio_deinit` + `pinMode(INPUT_PULLUP)`. IDF 5.5 may hold the pads when RTC_PERIPH is off [HWF F15]; startup also releases ext1 pins [HWF F14]. The calls are cheap insurance.
- `esp_sleep_get_ext1_wakeup_status()` names the key; it is logged (`woke by K2/GPIO5, sample 212 ms`) and swallowed if it is still down (§2.3.2).
- **Phantom-wake guard** (D27, `isPhantomWake()`, pure). A wake is **phantom** only if all hold: the cause is EXT1; no key reads LOW at the boot sample; **and** the previous deep sleep lasted less than `kPhantomMaxSleepMs` = 3 s (`sleptUs(RtcState)`, measured from the sealed snapshot to now). A brisk tap after a normal sleep is therefore never phantom, even when the key is released before the sample (a warm boot takes up to ~0.3 s [EST], a tap 80–200 ms). Phantom wakes are counted in `RtcState.phantomWakes` inside a window of `kPhantomWindowMs` = 2 min that starts at the first one (`phantomWindowStart`); a wake outside the window restarts the count. After `kPhantomWakeLimit` = 5 inside the window the woken pin(s) are excluded from the next ext1 mask, `E-KEY-STUCK` is logged and the KEY STUCK card (phantom variant) explains it. The count resets on any accepted key press. `WakeInfo.sampleUs` is shown in Diagnostics › System and the boot report so the 3 s threshold can be tuned (§11 step 14) [HW?].
- TIMER wake → no splash, display stays off, the model restores, the alert fires on time. If the verdict is not `Restore`, or no Running/Done countdown came back, the display turns on with the TIMER LOST card (`E-RTC-LOST`, D32) instead of staying dark.

### 7.6 Board extras

- **GPIO48** drives both a red LED and a WS2812 DIN on the reference schematic [HWF SM-09]. With `kForceLed48Off = true`, boot calls `rgbLedWrite(48, 0, 0, 0)` once; the pin idles LOW. Not used otherwise in v2.0 [HW?].
- GPIO3 may carry a VBUS/2 divider (≈ 20 µA) [HWF SM-14]; it is not used.

### 7.7 Current estimates (**[EST] — every number is to be replaced by §11 step 19**)

| Item | Estimate | Basis |
|---|---|---|
| ESP32-S3 at 80 MHz, idle loop | 22 mA (clocks gated) … 36 mA (all peripheral clocks on) | datasheet Table 5-9 [HWF F32] |
| ESP32-S3 light sleep | 240 µA | [HWF F12] |
| ESP32-S3 deep sleep, RTC_PERIPH on | 8 µA | [HWF F12] |
| ME6217 LDO quiescent | 100 µA typ | [HWF SM-11] |
| WS2812 on GPIO48, idle | 0.4–1 mA, if fitted | community figures [HWF SM-17] |
| SSD1306 module on, Home at Mid | 6–12 mA | [EST] |
| SSD1306 off + charge pump off | ≤ 10 µA (controller) | [HWF U12] |
| SHT4x idle / measuring | 0.08 µA / 320 µA for 8 ms | [HWF F12] |
| SHT4x heater 110 mW | 33 mA for 1 s | [HWF F08] |
| QMC5883L 50 Hz OSR512 / standby | 250 µA / 3 µA | [HWF Q20/Q19] |
| QMC5883P Normal 50 Hz / suspend | 310 µA / 22 µA | [HWF P-CURRENT] |
| A key held during deep sleep | 73 µA per key | [HWF F13] |
| **Screen on, Home** | ≈ 30–50 mA | sum |
| USB probe windows while dark (D28) | ≈ +0.25 mA (300 ms awake every ≈ 32 s) | [EST] |
| **Display off, light sleep** | ≈ 0.65–1.65 mA (WS2812 dominates if present) | sum |
| **Power off (deep sleep)** | ≈ 0.12–1.1 mA (LDO + WS2812 dominate) | sum |

---

## 8. Settings

### 8.1 Complete list (19 rows; schema table `kSettingDefs[]` in `core/settings.cpp` drives the list UI, validation, the Serial `GET`/`SET`, and the README table)

| # | Section | Label | Kind | NVS key (type) | Values / range | Default | Applies |
|---|---|---|---|---|---|---|---|
| 1 | CLOCK | Time zone | Enum | `tz` (string id ≤ 4) | 18 zones §5.5 | `MSK` | on save; preview: header clock in the picker |
| 2 | CLOCK | Set time | Action | — | editor §2.5 | — | Manual quality |
| 3 | DISPLAY | Screen off | Enum | `scr` (u8 index) | 10 s, 20 s, 30 s, 1 min, 2 min, 5 min, Never | 30 s | on save |
| 4 | DISPLAY | Brightness | Enum | `bri` (u8) | Low / Mid / High (§3.7) | Mid | live preview |
| 5 | DISPLAY | Animations | Enum | `anim` (u8) | On / Off | On | on save |
| 6 | POWER | Auto power off | Enum | `aoff` (u8 index) | 5, 10, 30, 60 min after screen off; Never | 30 min | on save |
| 7 | POWER | Light sleep | Enum | `lslp` (u8) | On (never with a USB host) / Off | On | on save |
| 8 | SENSOR | Temp unit | Enum | `tunit` (u8) | °C / °F | °C | live preview |
| 9 | SENSOR | Temp offset | Number | `toff` (i8, 0.1 °C) | −5.0 … +5.0, step 0.1, accel | 0.0 | live preview (sub-label `Now 23.1 C`); RH corrected (§6.2) |
| 10 | SENSOR | Heater | Enum | `heat` (u8) | Off / Auto (RH ≥ 90 %) / Every 5 / 10 / 30 / 60 min | Off | next cycle; `N/A` when not SHT4x (sub-label `SHT3x has no pulse heater`); sub-label states the rule |
| 11 | SENSOR | Heater power | Enum | `hpw` (u8) | Low 20 mW / Mid 110 mW / High 200 mW (1 s pulse) | Mid | next pulse; N/A on SHT3x |
| 12 | COMPASS | Declination | Number | `decl` (i8, °) | −30 … +30, step 1, accel; E positive | 0 | live; sub-label `Moscow +12, see NOAA` |
| 13 | COMPASS | Mount | Enum | `mnt` (u8) | 0°, 90°, 180°, 270° × chip up/down (8) | 0° up | live heading preview in the sub-label |
| 14 | COMPASS | Needle | Enum | `ndl` (u8) | Calm / Normal / Fast (§3.6, §6.4) | Normal | live |
| 15 | COMPASS | Calibrate | Action | (`cal` blob) | §6.4 | — | value `Good 94%` / `None` / `N/A` |
| 16 | SYSTEM | Diagnostics | Action | — | §9.5 | — | — |
| 17 | SYSTEM | Power off | ActionHold | — | hold `*` 1.5 s | — | deep sleep (manual) |
| 18 | SYSTEM | Reset settings | ActionHold | — | hold `*` 1.5 s | — | erases the 13 setting keys + `tmpre`; **keeps** `cal`, `clk`, `bld`; reboots |
| 19 | SYSTEM | Version | Info | — | `2.0.0` | — | not selectable for editing (no `*` icon) |

The "Values / range" column lists the options, not their on-screen text: list rows show the short forms and pickers the picker forms of §3.8.5 (`ValueStyle`), all ASCII with `°` = `kDegChar`, generated by sweeps that the layout audit checks for every option and for each number's min and max (§10.2).

Hidden stored values: `tmpre` (u8, last timer preset, default 2 = 5 min), `cal` (blob §6.4), `clk` (blob), `bld` (u32), `ver` (u8 schema = 1), `boots` (u32), `rsts` (8-byte ring of the last reset reasons).

### 8.2 Persistence (NVS via `Preferences`, namespace `hg2`; v1 used other storage, so nothing collides)

- **One key per setting.** A missing key → default. On load, every value passes `isValid()`. An invalid value → `sanitize()` (default), `W-CFG-RESET` logged with key and old value, and **one** card naming the first reset key (§3.8.8).
- **Schema version** `ver` (u8 = 1). Migration is a switch in `persist.cpp`: a new setting is a new key with a default; a removed key is ignored; a changed meaning bumps `ver` with a converter.
- **Blobs** `cal` and `clk` carry magic + version + CRC32. A failed check → ignored + logged (a bad `cal` blob shows as `I-MAG-UNCAL`; a bad `clk` blob counts as "no checkpoint").
- **When written**:
  - a setting: immediately when its picker is saved (one small key);
  - `tmpre`: on timer start;
  - `cal`: on calibration save;
  - `clk`: §5.4;
  - `boots`/`rsts`: once per boot.
  - Never during `^`/`v` scrolling: previews live in RAM.
- **Wear**: user-driven writes plus ~24 clock checkpoints/day of ~32 B. NVS spreads them over 5 pages × 126 entries of the 20 KB partition; negligible against 100 k erase cycles [EST].
- **NVS failure** (`Preferences.begin` false): `E-NVS` FAULT card; settings live in RAM only for this boot.
- **Write failure** (`IKvStore::put()` returns false, e.g. a full or damaged partition): the new value stays applied for this session, toast "Not stored: storage" (never "Saved"), `E NVS  put '<key>' failed` logged every time, `E-NVS` raised (FAULT card once per boot). The same applies to `tmpre`, `cal` (the calibration RESULT screen still shows the grade, plus the toast "Not stored: storage") and `clk` (log only).
- Uploads keep NVS unless "Erase All Flash Before Sketch Upload" is enabled (§10.6).

---

## 9. Diagnostics

### 9.1 Serial log

- Format: `[<uptime s.mmm>] <E|W|I|D> <TAG4> <message>`, e.g. `[   12.345] I SHT  SHT4x @0x44 sn=0F3A21C4`.
- Tags: `BOOT I2C0 I2C1 OLED SHT  MAG  KEY  UI   CLK  PWR  CFG  CMD  BAT  SW   TMR`.
- Error lines carry the code: `[    0.412] E MAG  E-MAG-NONE nothing at 0x0D/0x2C/0x1E; bus saw: 0x44`.
- Default level I. `LOG D` adds key events (`D KEY  # down t=12.300 -> SwLap`) and frame timing.
- Every line goes into `LogRing` (48 × 96 B) and to Serial through the paced `Logger` (§7.3); `LOG DUMP` replays the ring line by line; lost output is reported as `[N bytes dropped]`.
- Telemetry (`TLM <s>`, off by default): `TLM t=23.41 rh=56.2 dp=14.1 hdg=045.2 Bh=16.6uT raw=-1234,2051,-5810 sw=RUN 754.12 tmr=RUN 277 pwr=ON page=COMPASS i2c=0/0 loop=1.2ms`.

### 9.2 Boot report (always; replayed by `INFO` and on host connect)

```
=== Hiking Gadget 2.0.0  build "Oct 11 2026 19:30:12" id 1880C947  core 3.3.12 / IDF 5.5.5 ===
chip ESP32-S3 rev v0.2  flash 4MB  cpu 80MHz  heap 271k  cdc_on_boot=1 usb_mode=hwcdc
reset POWERON  wake -  sample 212ms  boots 17  last resets: SW,DEEPSLEEP,...  rtc_state none  restore FRESH
keys K1=GPIO4(^) K2=GPIO5(v) K3=GPIO6(#) K4=GPIO1(*)  levels(1=up) 1111  stuck -
i2c0 SDA8 SCL7 100k  lines ok  scan: 0D 44
i2c1 SDA10 SCL11 400k lines ok  scan: 3C
oled SSD1306 @3C OK
temp SHT4x @44 sn=0F3A21C4 OK
mag  QMC5883L @0D id=FF OK  cal=GOOD 94% (2026-10-11) mount=0 decl=+0
bat  not fitted (kPinBatteryAdc = -1)
time APPROX build 2026-10-11 19:30:42 MSK  -> run tools/sync_time.py or send "TIME <unix>"
cfg  ver1 OK tz=MSK scr=30s bri=Mid heat=Off
faults: none
=== ready in 412 ms ===
```

### 9.3 Console commands

Line-based, case-insensitive, LF/CR/CRLF, ≤ 96 characters. Replies `OK …` or `ERR <code> <text>`. Every `ERR` also shows a toast.

| Command | Action |
|---|---|
| `PING` | `PONG HG2 2.0.0 <uptime_ms>` |
| `HELP`, `INFO` | command list; boot report + current state |
| `TIME <unix>[.ms]`, `TIME YYYY-MM-DD HH:MM[:SS]`, `TIME?` (alias `T`) | §5.6 |
| `TZ <id>`, `TZ?` | set zone (stable id, e.g. `MSK`); list zones |
| `SCAN` | scan both buses |
| `SHT` | one measurement with raw words and CRC status |
| `HEAT` | one heater pulse (guarded by `heaterDue`), reply `HEAT pre=… hot=… d=…` |
| `MAG <s>` | stream raw CSV `ms,x,y,z,ovf` for s seconds (≤ 60; for offline calibration analysis) |
| `KEYS` | raw levels + mapping |
| `GET <key>`, `SET <key> <value>` | settings via the schema (same validation as the UI) |
| `LOG <E/W/I/D>`, `LOG DUMP`, `TLM <s>` (0 = off) | logging |
| `REBOOT`, `FACTORY YES` | restart; erase namespace `hg2` and restart |

### 9.4 Error codes (`core/health.cpp` table; README explains each in Russian)

| Code | Severity | Device | Meaning → on-screen hint |
|---|---|---|---|
| `E-OLED-NONE` | Fatal (headless) | OLED | No ACK at 0x3C/0x3D → Serial banner only |
| `E-OLED-IO` | Warn | OLED | Runtime transfer errors → re-init every 10 s |
| `E-SHT-NONE` | Fault | SHT | Nothing at 0x44/0x45 → `Check SDA8 SCL7 3V3` |
| `E-SHT-UNK` | Fault | SHT | ACK but not SHT4x/SHT3x → `Unknown chip at 0x44` |
| `E-SHT-CRC` / `E-SHT-IO` | Fault (after 3) | SHT | Bad data / no reply → `Check wiring, noise` |
| `I-SHT-3X` | Info | SHT | SHT3x found → heater N/A card once |
| `W-HEAT-HOT` | Warn | SHT | Heater skipped, T > 60 °C |
| `E-MAG-NONE` | Fault | Mag | Nothing at 0x0D/0x2C/0x1E → Compass card |
| `E-MAG-UNK` | Fault | Mag | Wrong ID → card shows address + ID |
| `W-MAG-ID` | Warn | Mag | QMC5883L ID ≠ 0xFF, assumed anyway |
| `E-MAG-STUCK` | Fault | Mag | No/identical data → re-init |
| `W-MAG-NOSTATUS` | Warn | Mag | 0x1E chip whose RDY bit never sets while data changes ("DA 5883") → timed reads (D34) |
| `W-MAG-SAT` | Warn | Mag | Overflow → `SATURATED`, `Metal near?` |
| `I-MAG-UNCAL` / `W-MAG-CALCHIP` | Info / Warn | Mag | No calibration / calibration for another chip → `UNCAL` |
| `E-I2C-STUCK` | Fatal | Bus0/1 | Line held low after recovery → FATAL card |
| `W-I2C-REC` | Warn | Bus0/1 | Recovery performed |
| `E-KEY-STUCK` | Warn | Keys | Key LOW for 10 s while awake → ignored until 1 s HIGH (card now); key low at sleep / phantom wake storm → excluded from wake (card at boot) |
| `E-NVS` | Fault | Nvs | NVS unavailable or a write failed → settings RAM-only / toast `Not stored: storage` |
| `W-CFG-RESET` | Warn | Nvs | Invalid setting value reset to default |
| `W-CLK-LOST` | Warn | Clock | Power lost: clock restored approximately or not set |
| `W-SYNC-REJ` | Warn | Clock | Sync rejected (reason in the toast) |
| `W-RST-CRASH` | Warn | Power | Restarted after a crash or WDT; state restored (toast `Restored after crash`), or not restored after 3 in a row (STATE LOST card). `Sw`/`Usb` resets are not crashes. |
| `W-BROWNOUT` | Warn | Power | Brown-out reset |
| `E-PWR-SLEEP` | Warn | Power | Deep sleep rejected → toast `Sleep failed`, stays awake |
| `E-RTC-LOST` | Fault | Power | Deep-sleep wake (or warm reset) whose `RtcState` is unusable → STATE LOST / TIMER LOST card, display on (D32) |

### 9.5 Diagnostics screen

Five pages (§3.8.7). **Page 3 (Buttons) is the acceptance test for the K1..K4 mapping and for rail alignment**: each physical key must light the rail box next to itself.

---
## 10. Host tooling, build and flashing

### 10.1 Host build (`tools/host/Makefile`)

```make
U8G2  ?= $(firstword $(wildcard /dev/shm/arduino-esp32/user/libraries/U8g2/src) /mnt/wd/arduino-esp32/user/libraries/U8g2/src)
BUILD ?= /dev/shm/arduino-esp32/build/hg2-host        # must start with /dev/shm/ or /mnt/wd/ (checked; never '/')
CXXFLAGS = -std=c++17 -O1 -g -DHG_HOST -Wall -Wextra -Werror -Isrc -I$(U8G2)/clib -Itools/host
# libu8g2.a: every $(U8G2)/clib/*.c except mui*, -O1 -w, built once (~3 s with -j16)
# hg_tests : src/core src/drivers src/ui src/app (NOT src/hal) + tools/host/fakes + tools/host/tests
# hg_render: same + fixtures.cpp + render_main.cpp
```

| Target | Result |
|---|---|
| `make test` | build and run `hg_tests`; non-zero exit on any failure |
| `make render` | `hg_render` writes `$(BUILD)/screens/NN_name.pbm`; `pbm2png.py` writes `$(BUILD)/screens/NN_name.png` (4×, OLED look) and `$(BUILD)/screens/contact_sheet.png`; `hg_render --bindings > $(BUILD)/BUTTONS.md`, `--bindings --lang ru > $(BUILD)/BUTTONS_RU.md`. **Never writes into `docs/`**, so parallel work packages cannot overwrite each other. |
| `make audit` | runs the layout audit and keymap test over every fixture **and every sweep** (§10.2); sweeps are audited, not written as PNGs |
| `make docs` | WP14 only: copies `$(BUILD)/screens/*.png` to `docs/screens/` and `BUTTONS*.md` to `docs/` |
| `make golden` | compare renders with `tools/host/golden/*.pbm`; `make golden-update` accepts the new renders (reviewed by a human) |
| `make check` | `test` + `render` + `golden` + `audit` |

Fixtures compute every derived field from the fixture UTC (weekday, local time, DST, "ends at"), never from hard-coded strings. 2026-10-11 is a Sunday.

### 10.2 Render scenarios (`fixtures.cpp`; name = `ViewModel` + `UiState` + `now`)

Minimum set (each must render with `Canvas::rejected() == 0`). Owner file in brackets.
- Home [`fixtures_a.cpp`, WP8]: `home_synced, home_bg_chips, home_approx, home_all_chips_approx` (widest date `MON 28 MAR`, zone `OMST`, Approx, ⚠ + bell + timer + stopwatch chips: exercises the §3.4 overflow steps), `home_notime, home_nosensor, home_unknown_sht, home_cold_heat, home_stale, home_details, home_hold_off`
- Stopwatch [WP8]: `sw_idle, sw_running, sw_paused, sw_paused_all_chips` (`PAUSED` + ⚠ + bell + timer chip → timer chip dropped), `sw_reset_hold, sw_hour, sw_10h, sw_max, sw_lap_999` (lap number cell, `99:59:59` lap time, `+HH:MM:SS` delta)
- Timer [WP8]: `tm_select, tm_running, tm_running_60` (`60 MIN` right column), `tm_paused_on, tm_paused_off, tm_hour, tm_done, tm_done_overtime_1h` (Mid overtime), `alert_a, alert_b`
- Compass [`fixtures_b.cpp`, WP9]: `cmp_ok, cmp_true, cmp_target, cmp_target_rel180, cmp_target_rel270, cmp_on_course, cmp_uncal, cmp_field, cmp_saturated, cmp_none, cmp_unknown, cmp_chips`
- Settings [WP9]: `set_cover, set_list_top, set_list_sensor, set_list_system_hold, pick_tz, pick_tz_yekaterinburg` (Label fallback), `pick_scr, pick_decl, pick_mount, pick_heater, pick_heater_na, pick_toff, set_time_hh, set_time_year`
- Calibration [WP9]: `cal_start, cal_progress, cal_keep_level, cal_metal_near, cal_ready, cal_good, cal_failed`
- Diagnostics [WP9]: `diag_sensors, diag_i2c, diag_buttons, diag_mag, diag_system`
- Cards and system [WP8]: `card_check_hw, card_check_hw_multi` (3 failures → `+N more, see Serial`), `card_i2c_stuck, card_key_stuck, card_key_stuck_runtime, card_state_lost, card_timer_lost, card_cfg_reset, card_sht3x_info, power_off, power_off_timer, splash`
- Motion and toasts [`fixtures_nav.cpp`, WP10]: `slide_home_sw_t25, slide_home_sw_t50, slide_sw_home_t50, toast_hint, toast_longest` (the widest toast template with its worst argument), `toast_queue_under_card` (queue paused while a card shows)

**Sweeps** (audited by `make audit`, not rendered to PNG). Fixtures generated in loops so that overflows are caught mechanically, not by picking typical values:
- every `SettingId` × every enum option and the min/max of every number: the list row (with the cursor on it, and not), the cover row where applicable, and the picker (value, sub-label, both neighbours) [WP9];
- every time zone (std and DST dates): Home top line, list, picker [WP8 Home, WP9 picker];
- Home top line: all 2⁵ combinations of ⚠/bell/timer/stopwatch/`SYNC?` × widest and narrowest date × widest and narrowest zone [WP8];
- every carousel page's status line × all chip combinations × its tags [WP8/WP9];
- timer: every preset × Select/Running/Paused/Done, totals 1, 99, 100, 600 min, remaining 1 s / 59:59 / 10:00:00, overtime 0 s / 59:59 / 1:00:00 / 99:59:59 [WP8];
- stopwatch: elapsed 0, 59:59.99, 9:59:59.9, 99:59:59; lapNo 1, 9, 99, 100, 999; deltas at each format boundary, both signs [WP8];
- compass: target relative bearing 0..345° step 15° (24 states), heading 0..359° step 1° for the right column [WP9];
- every card with its longest variant; every `ErrCode`'s `errTitle`/`errHint` [WP8].

The prototype images (`/mnt/wd/arduino-esp32/tmp/spec_final/proto/contact_sheet.png` [V-render]) are background reading only, not an acceptance reference (D37).

### 10.3 Unit tests (`tools/host/tests/test_*.cpp`, `tiny_test.h`, no external framework)

| File | Cases (vectors in §5.5, §6.2, Appendix B) |
|---|---|
| `test_civil_time` | days↔civil round trip 1970..2100; leap years; weekday (2026-10-11 = Sun); `parseBuildStamp`; `parseLocalDateTime`; `parseUnixMs` with fraction |
| `test_tz` | every table entry; EU/US boundaries 2026–2030 at ±1 s; IST +5:30; `localToUtc` gap and overlap |
| `test_clock_policy` | every row of §5.3; `validateSync` boundaries; build-ID gate (same id → no build candidate) |
| `test_stopwatch` | start/pause/resume/lap/reset; lap deltas; 99-lap ring with numbering past 99; cap auto-pause; snapshot round trip |
| `test_countdown` | preset cycle; ceil display (4 999 999 µs → 5 s, 1 µs → 1 s); `tick()` exactly once; +1 min cap; pause math; overtime; restart |
| `test_rtc_bridge` | seal/verify; CRC corruption; `rtcMagicOnly`; `decideRestore` for every row of §5.4 (incl. `Usb`/`Sw` non-crash, layout bump → `Lost`, DEEPSLEEP invalid → `Lost`, crash loop); 10-min simulated deep sleep (stopwatch +600 s; timer expires during sleep → alert once with correct overtime); `isPhantomWake` (tap released before the sample after a 10 min sleep → not phantom; no key LOW after a 1 s sleep → phantom; 5 in 2 min → exclusion; 4 in 2 min + 1 after 3 min → no exclusion) |
| `test_input` | 1–20 ms chatter → 1 press; 2-sample rule; `tDown` = first LOW; click vs hold vs hesitation; repeat 400/125/50 ms; accel ×5 after 2 s; swallow until release (and no effect on a key already up); first-key-owns; capture-all. **Latching** (D25): `configure()` after Down does not change the press; OnDown press then reconfigured to {OnClick, 800} and held 1 s → only `Down, Short, Up` (no Click-Short, no Long, no hold fill); OnDown + repeat press keeps repeating. **Stuck** (D26): 10 s LOW → `Stuck`, ownership released (another key's press works), `downMask()` excludes it; 1 s HIGH → `Unstuck`; 0.9 s HIGH then LOW again → still stuck; no stuck rule in capture-all |
| `test_env_math` | CRC vectors; conversions; RH clamp; RH correction 56.38 %; dew point 13.85 °C; AH 11.75 g/m³; °F |
| `test_heater_policy` | Off/Auto/EveryN; 60 s spacing; T > 60 °C skip; unsupported chip; busy; `shutdownPending` → `SkipShutdown` even when a pulse is due |
| `test_sht_driver` (FakeI2c) | SHT4x happy path with exact bytes; SHT3x detection only after SHT4x ruled out (assert **no 0x24 byte** is ever sent before that, and none in the cached-SHT3x check); SHT3x identified ≤ 100 ms after the address ACK; unknown at 0x44; none; `busyWaitUs` honoured before the first write; cached-chip check sends only 0xFD (SHT4x) / 0xF32D (SHT3x) and falls back to the full probe on failure; CRC error counting; heater → hot sample flagged |
| `test_mag_driver` (FakeI2c) | probe order and IDs; QMC5883L ID ≠ 0xFF → warning; exact init byte sequences per chip (incl. 0x0A = 0x00 INT_ENB and QMC5883P suspend before Normal); status-before-data read order; HMC X-Z-Y BE decode; QMC5883P axis rotation; overflow; stuck detection; standby bytes; **dead status** (D34): a 0x1E FakeI2c device with status stuck at 0 and changing data → `noStatus()`, timed reads every 67 ms, `W-MAG-NOSTATUS`, no `E-MAG-STUCK` restarts; status stuck at 0 and frozen data → `E-MAG-STUCK` |
| `test_compass_math` | 8 mounts × 24 headings with a synthetic field; declination wrap; EMA across 359/0; spring shortest path and settling < 0.6 s at ω 12; `point16`; hysteresis |
| `test_mag_calibrator` | synthetic ellipse (offsets ±500, scales 0.8/1.2, noise, 5 % outliers) → recovered within 2 %, residual heading error < 1°; partial turn → not Ready; tilted samples (±15° on half the turn) → rejected and `KeepLevel`; weak field → Poor |
| `test_settings` | defaults valid; every enum/number edge; `stepValue` clamp and accel; format strings; `editable` (Heater on SHT3x); tz id round trip |
| `test_persist` (FakeKv) | missing keys → defaults; invalid value → sanitized + one card; `cal`/`clk` CRC; reset keeps `cal`/`clk`/`bld` |
| `test_power_policy` | DIM/OFF timeline per timeout; never-timeout; light-sleep eligibility matrix (USB debounced, `usbRaw`, `usbPending`, probe window, key, deadline < 50 ms, setting); a stuck key (`keyDown = false`) does not block the timeout or light sleep; auto-off vetoes; `UsbDebouncer` flap patterns (on 1 s / off 3 s) and `pending()` |
| `test_cli` | every command form; garbage → `Error` with a reason; length limit |
| `test_compositor` | slide at d = 0, 1, 7, 8, 63, 64 in both directions vs a reference implementation; rail columns untouched |
| `test_oled_setup` (WP12, host) | with a recording byte callback: `oledSetup(addr7 = 0x3D)` → every transfer goes to 0x3D (fails if the address is set before setup); 0x3C likewise; flip → `0xA0 0xC0` sent after init |
| `test_logger` (WP11, FakeSerial with a 256-byte ring) | boot report and `LOG DUMP` arrive complete over several loop iterations; order kept; live-line overflow → `[N bytes dropped]` printed once output resumes; `write()` short counts handled |
| `test_frame_diff` | random buffer pairs → only changed tile spans; full refresh flag |
| `test_canvas` | ink-top offset = 1 for each of the 9 fonts (measured by rendering); `numWidth` stable for `"11:11"` vs `"88:88"`; negative coordinates rejected and counted; signed `line()` clipping |
| `test_keymap` | for **every** fixture: no icon ⇒ no short and no long action; long action ⇒ label or hold icon; OnDown short never with a long action; `^`/`v` act only when dots or chevrons are shown (`railHidden` exemption of §2.4: same short action on all keys, no long action, `S_ANY_KEY_OK` drawn); Bindings ↔ drawn rail are identical |
| `test_ui_nav` (WP10) | carousel `^`/`v` with wrap in both directions; a nav key during a slide jumps it to the end then starts the next; Home details toggle; Settings cover → list → picker → save/cancel → list → cover; set time field order and cancel on the first field; modal depth ≤ 2 (list → calibration → back to list); alert raised over a picker → dismiss returns to the picker with the pending edit intact; alert at carousel level → slides to Timer (Done); display-off reverts an uncommitted picker/set-time edit but keeps the modal; card queue order (CHECK HARDWARE, KEY STUCK, STATE LOST) and 8 s auto-dismiss; toast queue: 4 pending, merge of identical, pause under a card and while the display is off; captured binding (a press that opened the list does not act on the list); layer change between Down and Click drops the press |
| `test_layout_audit` | `DrawRecorder` over every fixture **and every sweep**: text inside 128×64 (descenders included); content ops x ≤ 116 when the rail is shown; rail ops x ≥ 119; no two text boxes intersect (except inside an active toast, pill or card scope); no text in a static keycap-label box except the label; no lowercase in `Micro`/`Label`; no text below the legibility floor except Micro in allowed roles (status line, tags, section headers, Diagnostics) |
| `test_app_scenarios` (fakes) | scripted runs of `AppCore::step()`: stopwatch across page changes, display-off and deep sleep; timer alert while on Compass, while display off, from deep sleep; wake swallow; settings save → NVS; sync accept/reject → toast; SHT disappears → card → comes back → toast; USB plugged blocks light sleep. Review 2 additions: **mark a target with a 1 s hold** → target stays set, no "Hold * to clear" toast; **open the list on the Reset settings row with a 2 s hold** → nothing is reset, no power-off; **K3 shorted LOW while awake** → KEY STUCK card at 10 s, display turns off at the timeout, light sleep entered with K3 excluded from the GPIO wake set, auto power-off happens, K3 released → toast; **USB plugged while dark** (raw true only after 150 ms awake) → detected within one probe window + 1 s, light sleep stops; **TIMER wake with a corrupt `RtcState`** → display on, TIMER LOST card, `E-RTC-LOST` logged; **picker save with `put()` failing** → toast "Not stored: storage", `E-NVS`; **manual power-off 2 s before the timer ends** → toast, alert fires, then power-off; **+1 min at 10 h** → toast "Max 10 h"; boot with Approx clock + CHECK HARDWARE card → both clock toasts shown after the card |
| `test_strings` | every string in `strings*.h` and `kTzTable` is printable ASCII and fully covered by its font (`u8g2_IsGlyph` for each char; `kDegChar` exempt, drawn as a primitive); every string fits its role's width limit (§4.10) with its worst-case argument, e.g. every `Toast` ≤ 105 px |

### 10.4 Lint (`tools/build.sh lint`, grep-based, fails the build)

- forbidden includes/calls in `src/core|drivers|ui|app` (§4.2 rule 1);
- `U8G2_.*2ND_HW_I2C` anywhere;
- `settimeofday|configTime|sntp_` outside `src/hal/hal_clock.cpp`;
- `u8g2_DrawLine` outside `src/ui/canvas.cpp`;
- `delay(` outside `src/hal`;
- `while *\( *!Serial`;
- `ESP_EXT1_WAKEUP_ALL_LOW`;
- `.ino` containing anything except comments, includes, `setup` and `loop`;
- any byte outside 0x20..0x7E inside a string literal in `src/ui/strings*.h`, `src/ui/fmt.cpp` or `src/core/tz.cpp` (D29);
- `#warning` anywhere in `src/` (use `#error`; D35), and any `hal/*.cpp` whose first include is not `build_checks.h`;
- `U8G2_R2` anywhere (use `kOledFlip`, §6.6).

### 10.5 `tools/build.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
[ -d /dev/shm/arduino-esp32 ] || /mnt/wd/arduino-esp32/ramcache.sh          # tmpfs vanishes after a reboot
export TMPDIR=/dev/shm/arduino-esp32/tmp; mkdir -p "$TMPDIR"
A="/mnt/wd/arduino-esp32/bin/arduino-cli --config-file /mnt/wd/arduino-esp32/arduino-cli.yaml"
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc"                                  # USBMode default = hwcdc [V-src boards.txt]
BP="/dev/shm/arduino-esp32/build/${HG_BUILDER:-hg2-main}"                  # one build path per parallel builder
case "${1:-all}" in
  fw)     $A compile --fqbn "$FQBN" --build-path "$BP" --warnings all "$ROOT" 2>&1 | grep -v 'Error initializing instance\|403' ;;
  probe)  $A compile --fqbn "$FQBN" --build-path "$BP-probe" --warnings all "$ROOT/tools/probe/api_probe" ;;
  host|test|render|golden|check) make -C "$ROOT" -f tools/host/Makefile "${1/host/all}" ;;
  lint)   "$ROOT/tools/lint.sh" ;;
  all)    "$0" lint && "$0" check && "$0" fw ;;
esac
```
- Never pass `build.extra_flags` (it carries the USB CDC defines) and never inject `-D` through `compiler.cpp.extra_flags` (a changed flag rebuilds the cached core) [V-src platform.txt].
- `Error initializing instance … 403` lines are noise from the blocked index (BRIEF §3).
- The full sketch build compiles warning-free from `src/` (third-party warnings tolerated).
- The script prints the size report. The `fw` exit status is the compile status (use `set -o pipefail`).

### 10.6 Flashing instructions for the user (README, in Russian; Arduino IDE 2.x on Windows)

1. **Boards Manager**: "esp32 by Espressif Systems" **3.3.x** (developed against 3.3.12). **Library Manager**: **U8g2** by olikraus (2.36.x or newer). No other libraries.
2. **Tools menu**:

   | Option | Value |
   |---|---|
   | Board | **ESP32S3 Dev Module** |
   | USB CDC On Boot | **Enabled** (otherwise Serial goes to the UART pins and the USB port is silent, v1 pitfall 3; the firmware refuses to compile with an `#error` that names this menu item, D35) |
   | USB Mode | Hardware CDC and JTAG |
   | Upload Mode | UART0 / Hardware CDC |
   | Flash Size | 4MB |
   | Partition Scheme | Default 4MB with spiffs |
   | PSRAM | **Disabled** (never "OPI PSRAM" on the FH4R2 [HWF SM-21]) |
   | Flash Mode | QIO 80MHz (never 120 MHz) |
   | CPU Frequency | any (the firmware sets 80 MHz) |
   | Erase All Flash Before Sketch Upload | **Disabled** (keeps settings and calibration) |

3. Open the `Hiking_Gadget` folder (the `.ino` must be at its root) and click Upload.
4. **If the port is missing or the upload fails** (gadget in deep sleep, or a crashed sketch): press any gadget key first. Otherwise hold **BOOT**, tap **RESET**, release BOOT, upload, then tap RESET [HWF SM-07].
5. Serial Monitor at 115200 (any baud works over CDC). The boot report appears, or type `INFO`.
6. **Time** (if the screen is off, **press any gadget key first**: a sleeping gadget is invisible to the PC for up to ~35 s, a powered-off one until a key is pressed):
   - close the Serial Monitor, then open `tools/sync/hg_sync.html` in Chrome or Edge → Connect; or
   - run `py tools\sync_time.py`; or
   - type `TIME 2026-10-11 19:42:00` in the Serial Monitor.
7. **Keys**: the README links `docs/BUTTONS_RU.md` (generated by `hg_render --bindings --lang ru`) and `docs/BUTTONS.md`; it never contains a hand-copied key table (v1 pitfall 7).

---

## 11. Hardware verification checklist (for the user)

Record each step as **Pass / Fail / Not checked** (not checked ≠ did not help), with the numbers asked for. Steps are ordered cheapest-first.

| # | Action | Expected | If not |
|---|---|---|---|
| 1 | Flash (§10.6), open Serial Monitor | boot report within 2 s, **complete** down to `=== ready in … ms ===`; copy it into the report | compile error naming "USB CDC On Boot" → fix the Tools menu (§10.6); silent → press RESET; report cut short → report it (tests §7.3 pacing) |
| 2 | Look at the screen after flashing | splash, then Home with time ≈ compile time and the `SYNC?` tag; text upright | upside-down → `kOledFlip = true`; image shifted 2 px or garbage at the right edge → `kOledController = Sh1106`; time far off → the IDE did not recompile the `.ino`, or the PC zone ≠ MSK |
| 3 | Settings › Diagnostics › Buttons; press `^`, `v`, `#`, `*` one at a time | rows `K1 UP`, `K2 DOWN`, `K3 HASH`, `K4 STAR` count up in that order | report which K lit for which symbol; fix `kKeyPins` only |
| 4 | Same page: does each lit rail box sit beside its physical key? | `v` and `#` boxes level with their keys; `^`/`*` read as corner hints | report "box N px above/below key" for `v` and `#` → tunes `kCapHashY`, `kCapStarY`, `kRailSlotY` |
| 5 | Diagnostics › I2C | bus 0: `44` + one of `0D`/`2C`/`1E`; bus 1: `3C` | wiring/power; report the exact list |
| 6 | Diagnostics › Sensors | `TEMP SHT4X 44 <serial>` (or SHT3X); `MAG <chip> <addr> ID <id>` | report the chip names and ID bytes |
| 7 | Breathe on the T/RH sensor; after 10 min of screen-off compare T with a reference thermometer | RH rises within 5 s; T within ~1 °C | set Temp offset; report the difference |
| 8 | Diagnostics › Sensors › `*` (heater test, 110 mW), then watch Home | `HEAT pre>hot D+…` shows a rise of several °C; Home shows the flame and frozen values, resuming within 20–90 s | report the rise and the cool-down seconds (tunes §6.2) |
| 9 | Diagnostics › Sensors DRDY line; page 4 while rotating | QMC5883L ≈ 50 edges/s RISE (proves INT_ENB polarity); HMC ≈ 15/s FALL; QMC5883P `NO PIN`; raw X/Y/Z change smoothly, OVF 0 | report the DRDY line; DRDY = 0 is acceptable (polling) |
| 10 | Compass › `#` (calibrate): keep it **flat**, two slow turns | ring fills, `Ready`, `*` SAVE → `GOOD`/`OK` | report quality %, field µT and any `Keep level!` / `Metal near?` |
| 11 | Settings › Compass › Mount: point the top edge north (phone compass, away from metal), scroll until the live heading ≈ 0, save; then check E, S, W; turn clockwise | heading **increases** clockwise; error ≤ 10° at N/E/S/W | report the chosen option and the four errors; repeat at High vs Low brightness (OLED current near the sensor) |
| 12 | Stopwatch: start; change pages; let the screen turn off; wake; then hold `*` on Home (power off), wait 2 min, wake | runs throughout; laps intact; after power-off it shows +2 min more | report the difference vs a phone after 10 min |
| 13 | Timer: 1 min then switch to Compass; 1 min with the screen off; 3 min then power off | the alert appears on Compass; the display wakes; the gadget wakes itself and flashes | dismiss once with each key: no other action may fire |
| 14 | Power off, then wake with **each** of the 4 keys separately, first with a normal press, then with the briskest tap you can; repeat the brisk tap 6 times within 2 min after waiting ≥ 5 s each time | each key wakes; brisk taps never produce a KEY STUCK card; no spurious wakes over 10 min; note the `WAKE … +NNNMS` value in Diagnostics › System | tests ext1 ANY_LOW + RTC pull-ups (unverified in v1) and the phantom-wake rule (§7.5); try `kDeepSleepKeepRtcPeriph = false` once and report; report the wake sample times |
| 15 | With the Serial Monitor open: let the screen turn off. Then on a phone charger: let it sleep, press a key | with USB host: COM port stays alive (no light sleep); on a charger: wakes instantly, timer still fires | report |
| 16 | `sync_time.py`, then 1 h powered off, then `sync_time.py --check`; repeat with 1 h screen-off (light sleep) | drift in s/h for each | decides whether the RC is good enough (§5.1) |
| 17 | Brightness Low / Mid / High, indoors at night and in sun | three visibly different levels; Low readable at night | report; tunes §3.7 |
| 18 | Read the screen outdoors at arm's length | date, units, labels, messages readable | report any text that is too small (legibility floor) |
| 19 | Current with a USB meter or on the battery line: Home on, screen off, powered off | fill in §7.7 | report mA and whether the red LED / RGB LED is lit |
| 20 | Unplug power 10 s, re-plug | `SYNC?` + toast "Clock lost…" (time from the NVS checkpoint); settings and calibration intact | report |
| 21 | Pull the GY-273 SDA on Compass; reconnect. Same for the SHT on Home | FAULT card within ~1 s; recovers by itself with a toast | report |
| 22 | Diagnostics › System: `LOOP MAX` and `FLUSH` after using every page | loop max ≤ 12 ms; flush per span ≤ 5 ms | report (validates §4.5) |
| 23 | Let the screen turn off on battery/charger (light sleep). Then plug into the PC **without touching any key** and run `sync_time.py` (or open `hg_sync.html`) | the COM port appears within ~35 s and stays; sync succeeds without a key press | report the seconds until the port appeared, and whether Windows showed "USB device not recognized" (tests D28) |
| 24 | Hold `#` pressed for 12 s on Home (simulates a stuck key), then release | KEY STUCK card after 10 s naming K3/GPIO6; other keys still work; after release, toast "Key # works again"; with the key held the screen still times out | report (tests D26) |

---

## 12. Risks and open questions (only real hardware can answer)

| # | Risk / question | Mitigation in the design | Check |
|---|---|---|---|
| 1 | K1..K4 ↔ `^ v # *` mapping unverified; external pull-ups on the module unknown | one table, Buttons page, `KEYS` command, INPUT_PULLUP | §11.3 |
| 2 | Rail rows estimated from a marketing render | theme constants; Buttons page draws the slots | §11.4 |
| 3 | SHT45 vs SHT3x ("GY-SHT31-D" listing) | autodetect, SHT4x-first probe, honest heater N/A | §11.6 |
| 4 | QMC5883L / QMC5883P / HMC5883L / "DA 5883" (0x1E, HMC-like map, broken status [HWF Q32]) | ID-based probe; unknown ID shown on screen with the byte. The "DA 5883" passes the `H43` ID check, so the ID alone does not catch it: a dead RDY bit with changing data switches to timed reads + `W-MAG-NOSTATUS` (D34) | §11.6, §11.9 |
| 5 | QMC5883L INT_ENB polarity; DRDY wiring | polling only; DRDY counter is diagnostic | §11.9 |
| 6 | QMC5883P `0x29 = 0x06` meaning and P-vs-L axis rotation | datasheet sequence (flag); board-frame conversion; mount picker absorbs residual sign issues | §11.11 |
| 7 | GY-273 axis orientation in the enclosure; nearby iron | 8-option mount with live preview; calibrate in the final enclosure; `FIELD?` | §11.10–11 |
| 8 | Tilt error ≈ 3°/° at Moscow inclination; no accelerometer | "Hold level", level-only calibration with tilt rejection | §11.11 |
| 9 | RC slow-clock drift in light and deep sleep (no 32 kHz crystal) | sync age, drift reported on sync, `clock_trim_ppm` hook | §11.16 |
| 10 | ext1 ANY_LOW with RTC pull-ups; RTC_PERIPH on/off; phantom wakes; wake-to-sample latency vs a brisk tap | explicit pulls, `hold_dis`/`deinit`, stuck-key exclusion, phantom-wake guard counting only short-sleep storms (D27); sample time logged | §11.14 |
| 11 | USB-Serial/JTAG during light sleep; `isPlugged()` flapping; DTR/RTS reset when a tool opens the port; a host plugged in while dark | no light sleep or auto-off with a host; 1 s/3 s debounce; no light sleep while raw/pending; 300 ms probe window every 8 slices (D28); DTR/RTS low; PING retry; "press any key first" in the tools | §11.15, §11.23 |
| 12 | Whether TWDT counts during manual light sleep | 4 s light-sleep slices | §11.15 (soak) |
| 13 | Panel: SSD1306 vs SH1106, 0x3C vs 0x3D, orientation, fast warm init without delays | config switches, address probe, `kOledFastWake` flag | §11.2 |
| 14 | Low-brightness triple (precharge/VCOMH) effect varies by panel | one table, three levels | §11.17 |
| 15 | Legibility of 7 px text outdoors | legibility floor; Micro only for secondary roles | §11.18 |
| 16 | Heater rise and cool-down on this breakout; effect of a 5–60 min interval on creep (unvalidated by Sensirion) | adaptive cool-down with a cap; honest sub-label | §11.8 |
| 17 | Board BOM (LDO Iq, WS2812 idle current, VBUS divider) dominates sleep current | GPIO48 forced off; documented | §11.19 |
| 18 | Self-heating of the SHT by the ESP32, OLED and LDO in the enclosure | temp offset with RH correction | §11.7 |
| 19 | Does the Arduino IDE always recompile the `.ino` (fresh `__TIME__`)? | build-ID gate; Approx flag | §11.2 |
| 20 | Real flush time with IDF `i2c_master` per-transaction overhead | ≤ 2 spans/iteration, measured in Diagnostics | §11.22 |
| 21 | Brown-out on a weak battery during a 200 mW pulse with the OLED on | default Mid 110 mW; `W-BROWNOUT` | — |
| 22 | A key shorted or held LOW while awake (pocket pressure, moisture) | runtime stuck rule after 10 s: key ignored, card, normal timeout and sleep (D26) | §11.24 |
| 23 | Whether a busy SHT4x accepts 0x94 during a heater pulse (undocumented) | `shtBusyUntil` wait after warm resets; one-time `Unknown` then re-probe if `RtcState` is lost (D33) | §11.8 (reboot during a heater test) |

---
## 13. Implementation plan (work packages for parallel agents)

Rules for every agent:
- **Do not spawn sub-agents; work yourself** (user rule).
- Touch only the files your WP owns. A header owned by WP0 may be changed only through a spec change.
- Scratch and builds go to `/dev/shm/arduino-esp32/...` or `/mnt/wd/arduino-esp32/tmp/...`, never to `/`. Use your own `HG_BUILDER` build path.
- Every WP ends green on its acceptance commands. "Not run" is reported as not run, never as passed.

### 13.1 Dependency graph

```
WP0 (contracts + skeleton) ──┬─▶ WP1 time ───────────┐
                             ├─▶ WP2 models+input ───┤
                             ├─▶ WP3 compass math ───┤
                             ├─▶ WP4 SHT ────────────┤
                             ├─▶ WP5 magnetometer ───┤
                             ├─▶ WP6 settings/power/health/cli ─┤
                             ├─▶ WP7 UI foundation ──┬─▶ WP8 screens A ─┐
                             │                       ├─▶ WP9 screens B ─┤
                             │                       └─▶ WP10 UI controller ─┤
                             ├─▶ WP11 app core (uses fakes; links stubs until WP1–10 land) ─┤
                             ├─▶ WP12 HAL (ESP32) ───┤
                             └─▶ WP13 tools + docs ──┴─▶ WP14 integration
```
WP1–WP13 can start in parallel the moment WP0 is merged. Before a dependency lands, each WP compiles against WP0's stub `.cpp` files.

### 13.2 Work packages

| WP | Owns (files) | Depends on | Content | Acceptance |
|---|---|---|---|---|
| **WP0** Contracts & skeleton | `Hiking_Gadget.ino`; `src/config/*`; every header listed in §4.4 (`core/*.h` incl. the new `types.h` enums, `drivers/i2c_bus.h`, `drivers/sht.h`, `drivers/mag.h`, `ui/u8g2_inc.h`, `ui/canvas.h`, `ui/bindings.h`, `ui/view_types.h`, `ui/screen.h`, `ui/view_model.h`, `ui/display_port.h`, `ui/oled_setup.h`, `ui/ui_controller.h`, `ui/theme.h`, `ui/icons.h`, `ui/strings.h/.cpp` (X-macro frame, §4.10), `ui/screens/registry.h/.cpp`, `app/ports.h`, `app/app_core.h`); **`drivers/i2c_bus.cpp` implemented in full** (register helpers are real code); stub `.cpp` for every other header returning defaults; **handed over after WP0** (created by WP0 as empty/stub files, then owned by the named WP): `ui/strings_core.h` (WP7), `ui/strings_screens_a.h` + `ui/screens/registry_a.cpp` (WP8), `ui/strings_screens_b.h` + `ui/screens/registry_b.cpp` (WP9); `tools/host/Makefile`, `tools/host/tiny_test.h`, `tools/build.sh`, `tools/lint.sh`, `tools/host/fakes/fake_i2c.{h,cpp}` (scripted bus with a `FakeI2cDevice` hook interface, used by WP4/WP5/WP11) | — | Directory tree §4.1; headers verbatim from §4.4 and Appendix A; include graph of §4.4; build plumbing | `tools/build.sh lint` passes; every §4.4 header compiles alone (one `.cpp` per header, host); `make -f tools/host/Makefile test` builds and runs 0 tests; `tools/build.sh fw` compiles (stubs) with no warnings from `src/` |
| **WP1** Time | `core/civil_time.cpp`, `core/tz.cpp`, `core/clock_policy.cpp`, `tests/test_civil_time.cpp`, `tests/test_tz.cpp`, `tests/test_clock_policy.cpp` | WP0 | §5.2–5.5 | `make test` green; all §5.5 vectors at ±1 s |
| **WP2** Models & input | `core/stopwatch.cpp`, `core/countdown.cpp`, `core/rtc_bridge.cpp`, `core/input.cpp`, `core/crc.cpp`, tests `test_stopwatch`, `test_countdown`, `test_rtc_bridge`, `test_input` | WP0 | §2.2 (latching, stuck keys), §2.5, §5.4, §7.5 phantom rule | `make test` green |
| **WP3** Compass math | `core/compass_math.cpp`, `core/mag_calibrator.cpp`, tests `test_compass_math`, `test_mag_calibrator` | WP0 | §6.4 | `make test` green; synthetic-ellipse residual < 1°; tilt rejection works |
| **WP4** SHT | `core/env_math.cpp`, `core/heater_policy.cpp`, `drivers/sht.cpp`, `fakes/fake_sht.{h,cpp}` (FakeI2c device model), tests `test_env_math`, `test_heater_policy`, `test_sht_driver` | WP0 | §6.2 | `make test` green; the "no 0x24 before SHT4x ruled out" assertion holds |
| **WP5** Magnetometer | `drivers/mag.cpp`, `drivers/mag_chips.{h,cpp}`, `fakes/fake_mag.{h,cpp}`, `test_mag_driver` | WP0 | §6.3 | `make test` green; exact byte sequences per chip |
| **WP6** Settings, power, health, CLI, log | `core/settings.cpp`, `core/power_policy.cpp`, `core/health.cpp`, `core/cli.cpp`, `core/log_ring.cpp`, tests `test_settings`, `test_power_policy`, `test_cli` | WP0 | §7.2, §8, §9.3, §9.4 | `make test` green |
| **WP7** UI foundation | `ui/canvas.cpp` (incl. `kDegChar`), `ui/icons.cpp`, `ui/fmt.{h,cpp}`, `ui/err_text.cpp`, `ui/widgets.{h,cpp}` (incl. the status-line/Home top-line budget of §3.4 and the toast queue drawing), `ui/compositor.{h,cpp}`, `ui/frame_diff.{h,cpp}`, `ui/bindings.cpp`, `ui/strings_core.h` (content), `tools/host/fixtures.{h,cpp}` (registry + helpers + sweep helpers; fixtures self-register from `fixtures_a/b/nav.cpp`), `tools/host/render_main.cpp` (calls the `--bindings` hook provided by WP10), `tools/host/pbm2png.py`, `tools/host/fakes/{draw_recorder,fake_display}.{h,cpp}`, tests `test_canvas`, `test_compositor`, `test_frame_diff`, `test_layout_audit` (framework), `test_strings` | WP0 | §3.2–3.7, §4.8, §4.10 | `make test render` green; `test_canvas` measures the ink-top offset 1 for all 9 fonts on the installed U8g2 2.36.19 |
| **WP8** Screens A | `ui/screens/{home,stopwatch,timer,alert,cards,power_off,splash}.{h,cpp}`, `ui/screens/registry_a.cpp`, `ui/strings_screens_a.h`, their fixtures and sweeps in `fixtures_a.cpp` | WP7 (headers from WP0 suffice to start) | §3.8.1–3.8.3, §3.8.8 | renders match §3.8 (positions, budgets, formats); `make audit` (layout audit + keymap test) green for these fixtures **and sweeps**; contact sheet reviewed by a human |
| **WP9** Screens B | `ui/screens/{compass,calibration,settings_cover,settings_list,picker,set_time,diagnostics}.{h,cpp}`, `ui/screens/registry_b.cpp`, `ui/strings_screens_b.h`, fixtures and sweeps in `fixtures_b.cpp` | WP7 | §3.8.4–3.8.7, §5.5 picker format | as WP8 |
| **WP10** UI controller | `ui/ui_controller.cpp` (captured bindings, card and toast queues), `tests/test_ui_nav.cpp`, `tests/test_keymap.cpp`, `tools/host/fixtures_nav.cpp` (slides, toasts), `tools/host/bindings_doc.{h,cpp}` (`hg_render --bindings [--lang ru]` → `$(BUILD)/BUTTONS.md`, `BUTTONS_RU.md`; the Russian action descriptions are doc-only strings in this file) | WP7; gets WP8/9 screens only via `screenFor()` | §2.2 latching (dispatch side), §2.3–2.5, §3.6, §4.5 flush | `make test` green incl. the `test_ui_nav` cases of §10.3; both BUTTONS files generated into `$(BUILD)`; slide and toast fixtures render |
| **WP11** App core | `app/*.cpp` + internal headers (`time_service`, `thermo_service`, `compass_service`, `power_manager`, `persist`, `console`, `logger`, `app_core`), `tools/host/fakes/{fake_clock,fake_keys,fake_kv,fake_rtc,fake_power,fake_serial}.{h,cpp}` (FakeSerial models a bounded TX ring with short writes; FakePower reports `usbHostRaw` per scripted time), tests `test_persist`, `test_logger`, `test_app_scenarios` | WP0 (links stubs); full scenarios after WP1–10 | §4.3, §4.5, §4.6, §5, §6.2 service rules, §7, §8.2, §9 | `make test` green including all scenarios in §10.3 |
| **WP12** HAL (ESP32) | `src/hal/*` (incl. `build_checks.h`, the `ResetReason` `static_assert`s and the `WakeCause` mapping), `ui/oled_setup.cpp`, `tests/test_oled_setup.cpp`, `tools/probe/api_probe/api_probe.ino` (adds `Serial.setTxBufferSize`, `Serial.availableForWrite`, `u8g2_SetFlipMode`) | WP0 | §4.6, §6.1 recovery, §6.6, §7.3–7.6 | `tools/build.sh probe` and `tools/build.sh fw` compile with no warnings from `src/`; `make test` green for `test_oled_setup`; a build with `CDCOnBoot=default` fails with the `build_checks.h` message (checked once, output pasted in the WP report); size report |
| **WP13** Tools & docs | `tools/sync_time.py`, `tools/sync/hg_sync.html`, `README.md` (Russian), `docs/HW_CHECKLIST.md` (§11 in Russian, with result columns) | WP0 (protocol §5.6, §9.3) | §5.6, §10.6, §11 | `python3 -m py_compile tools/sync_time.py`; the HTML page opens without console errors (static check); both tools print the "press any key on the gadget first" hint on failure; README covers §10.6 and the §9.4 code table and **links** `docs/BUTTONS_RU.md` (no hand-copied key table) |
| **WP14** Integration | golden images `tools/host/golden/*`, the `make docs` target and its outputs `docs/screens/*`, `docs/BUTTONS.md`, `docs/BUTTONS_RU.md`, fixes across WPs by agreement | all | full build + all tests + renders reviewed against §3.8 | `tools/build.sh all` green; `make docs` run once at the end; contact sheet reviewed; firmware size ≤ 600 KB; report of every [HW?] left for §11 |

### 13.3 Definition of done (whole project)

1. `tools/build.sh all` is green: lint, host tests, renders + golden, firmware compile with the BRIEF command.
2. Every state in §10.2 renders, and the layout audit and keymap test pass for all of them.
3. `docs/BUTTONS.md`, `docs/BUTTONS_RU.md` and `docs/screens/contact_sheet.png` are regenerated from code by `make docs` (WP14), and the README links them.
4. The README (Russian) has the flashing steps, the sync tools, the key table, the error codes and the checklist link.
5. Nothing is claimed "verified on hardware": the §11 checklist is the user's to run, and the hand-off lists every open [HW?].

---

## Appendix A. Configuration constants (`src/config/*.h`; the only place for these numbers)

```cpp
// board_pins.h
constexpr uint8_t kPinSensorSda = 8, kPinSensorScl = 7;        // Wire  (SHT + magnetometer)
constexpr uint8_t kPinOledSda   = 10, kPinOledScl  = 11;       // Wire1 (SSD1306)
constexpr uint8_t kPinMagDrdy   = 12;                          // diagnostics only
constexpr uint8_t kKeyPins[4]   = { 4 /*K1 ^*/, 5 /*K2 v*/, 6 /*K3 #*/, 1 /*K4 * */ };   // [HW?] verify with Diagnostics › Buttons
constexpr uint64_t kWakeMask    = (1ULL<<1) | (1ULL<<4) | (1ULL<<5) | (1ULL<<6);       // 0x72
constexpr int8_t  kPinBatteryAdc = -1;  constexpr float kBatteryDividerRatio = 2.0f;   // GPIO2 suggested
constexpr uint8_t kPinLed48 = 48;
constexpr uint8_t kShtAddrs[2] = {0x44, 0x45};
constexpr uint8_t kMagAddrQmcL = 0x0D, kMagAddrQmcP = 0x2C, kMagAddrHmc = 0x1E;
constexpr uint8_t kOledAddrs[2] = {0x3C, 0x3D};

// build_config.h
constexpr uint32_t kSensorI2cHz = 100000, kOledI2cHz = 400000;
constexpr uint16_t kI2cTimeoutMs = 20;
constexpr uint32_t kCpuMhz = 80;
constexpr int16_t  kBuildTzOffsetMin = 180;   constexpr int32_t kBuildToBootLagS = 30;
enum class OledController : uint8_t { Ssd1306, Sh1106 };
constexpr OledController kOledController = OledController::Ssd1306;
constexpr bool kOledFlip = false, kOledFastWake = true;   // kOledFlip: u8g2_SetFlipMode (never U8G2_R2), §6.6
constexpr bool kQmc5883pWriteSignReg = true;    // datasheet setup step (D13)
constexpr bool kDeepSleepKeepRtcPeriph = true;  // D14
constexpr bool kForceLed48Off = true;
constexpr size_t kSerialTxBufBytes = 4096, kLogTxFifoBytes = 1024;   // D31
// No #if/#error here: config/ is included by host code, where ESP_ARDUINO_VERSION_MAJOR is undefined (reads as 0).

// hal/build_checks.h (WP12; the FIRST include of every hal/*.cpp; device only)
#include <Arduino.h>                             // defines ESP_ARDUINO_VERSION_MAJOR
#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "Hiking Gadget v2 requires arduino-esp32 core 3.x (developed on 3.3.12)"
#endif
#if !ARDUINO_USB_CDC_ON_BOOT || !ARDUINO_USB_MODE  // -D flags from boards.txt menus [V-src platform.txt 79]
#error "Arduino IDE: Tools > USB CDC On Boot = Enabled, USB Mode = Hardware CDC and JTAG (see README)"
#endif

// timing.h (ms unless the name ends in Us)
kDebounceMs 15; kClickMaxMs 400; kHoldVisualMs 150; kHoldMs 800; kHoldCriticalMs 1500;
kRepeatDelayMs 400; kRepeatMs 125; kRepeatFastMs 50; kRepeatFastAfterMs 1500; kAccelAfterMs 2000; kAccelStep 5;
kPressedMinMs 80; kButtonsExitHoldMs 2000; kButtonsExitIdleMs 10000;
kDimLeadMs 5000; kDimLeadShortMs 3000; kFadeInMs 120; kFadeOutMs 250; kToastMs 1200; kToastSlideMs 100; kToastQueue 4; kSlideMs 180;
kAlertFlashMs 30000; kAlertFlashPeriodMs 500; kAlertHoldMs 300000;
kCardAutoDismissMs 8000; kSplashMinMs 600; kSplashMaxMs 1500; kPowerOffCardMs 1000;
kShtHomePeriodMs 2000; kShtOnPeriodMs 10000; kShtAutoDarkPeriodMs 60000; kShtStaleMs 60000;
kShtMeasWaitMs 10; kSht3xMeasWaitMs 16; kShtHeaterWaitMs 1110; kShtReadDeadlineMs 30; kShtHeaterDeadlineMs 90;
kShtProbeSerialRetryMs 10; kShtHeaterBusyMaxMs 1200; kHeaterMinSpacingMs 60000; kHeaterAutoMinGapMs 600000;
kHeaterCoolMinMs 20000; kHeaterCoolMaxMs 90000; kHeaterMaxAmbientC 60; kHeaterAutoRh 90;
kMagPollMs 20; kMagStandbyGraceMs 2000; kMagStuckMs 500; kMagStuckSamples 25; kMagNoStatusMs 500; kQmcResetWaitMs 10; kQmcPResetWaitMs 5;
kCalMinMs 8000; kCalTimeoutMs 120000; kCalKeepAwakeMaxMs 120000;
kReprobeBackoffMs {5000, 10000, 30000, 60000}; kI2cMaxRecoveriesPerMin 3;
kLightSleepMinMs 50; kLightSleepMaxSliceMs 4000; kTimerWakeLeadMs 1500; kAutoOffTimerVetoMs 60000;
kUsbPlugOnMs 1000; kUsbPlugOffMs 3000; kUsbProbeEverySlices 8; kUsbProbeAwakeMs 300; kKeyReleaseWaitMs 3000;
kPhantomWakeLimit 5; kPhantomWindowMs 120000; kPhantomMaxSleepMs 3000; kKeyStuckMs 10000; kKeyStuckClearMs 1000;
kClockCheckpointMs 3600000; kFullRefreshMs 60000; kFlushBudgetUs 8000; kFlushMaxSpans 2; kMaxIdleMs 5;
kCrashRestoreUptimeMs 60000; kCrashRestoreMax 3;
```

## Appendix B. Test vectors (computed in this session)

| Quantity | Input | Expected |
|---|---|---|
| CRC-8 Sensirion | `BE EF` / `66 66` / `80 00` / `00 00` / `FF FF` | `92` / `93` / `A2` / `81` / `AC` |
| CRC-32 IEEE | `"123456789"` | `0xCBF43926` |
| FNV-1a 32 | `"Oct 11 2026 19:30:12"` | `0x1880C947` |
| SHT4x T | `0x6666` | 25.00 °C |
| SHT4x RH / SHT3x RH | `0x8000` | 56.50 % / 50.00 % |
| RH correction | 50 % @ 25 °C → 23 °C | 56.38 % |
| Dew point | 25 °C / 50 %; 23.4 °C / 56 % | 13.85 °C; 14.12 °C |
| Absolute humidity | 23.4 °C / 56 %; 25 °C / 50 % | 11.75 g/m³; 11.48 g/m³ |
| Weekday | 2026-10-11 | Sunday |
| Unix → MSK | 1791736927 | 2026-10-11 19:42:07 MSK |
| Build stamp | `Oct 11 2026` `19:30:12` @ UTC+3 | 1791736212 (+30 s lag) |
| EU DST 2026 / 2027 | start / end | 1774746000 / 1792890000; 1806195600 / 1824944400 |
| US DST 2026 / 2027 (NYC) | start / end | 1772953200 / 1793512800; 1805007600 / 1825567200 |
| Heading convention | field along +F; device facing east (field along +L) | 0°; 90° |
| Moscow field (WMM2025) | 55.756 N 37.617 E | D = +12.1° E, I = 71.7°, H = 16.64 µT [HWF hmc F17] |

## Appendix C. Verification log (what was checked in this session, and how)

| Fact | How |
|---|---|
| All HAL APIs named in §4.6, §6.6, §7.3–7.5 exist and compile on core 3.3.12 | Probe sketch compiled, `--warnings all`, no warnings, 357,010 B / 25,400 B [V-build] |
| `HWCDC::flush()` discards the TX queue when `tx_timeout_ms == 0`; `isPlugged()` flaps; default timeout 100 ms | Read `cores/esp32/HWCDC.cpp` lines 105, 145–160, 263–290, 293–345, 629–660 [V-src] |
| `endTransmission()` mapping 0/2/5/4, never 3 | Read `libraries/Wire/src/Wire.cpp` 445–476 and `esp32-hal-i2c-ng.c` 237–285 [V-src] |
| `CONFIG_LIBC_TIME_SYSCALL_USE_RTC_HRT=y`, `CONFIG_ESP_TIME_FUNCS_USE_RTC_TIMER=y`, `…_USE_ESP_TIMER=y`, `CONFIG_RTC_CLK_SRC_INT_RC=y`, `CONFIG_RTC_CLK_CAL_CYCLES=576`, `CONFIG_PM_ENABLE` off, `CONFIG_ESP_TASK_WDT_TIMEOUT_S=5`, `CONFIG_LOG_MAXIMUM_LEVEL=1` | grep of `esp32s3-libs/3.3.12/sdkconfig` [V-src] |
| Effective C++ standard gnu++2a (both `-std=gnu++2b` and a later `-std=gnu++2a` in `cpp_flags`), `-fno-rtti` | `esp32s3-libs/3.3.12/flags/cpp_flags` [V-src] |
| `CDCOnBoot` default Disabled, `USBMode` default hwcdc; `compiler.cpp.extra_flags` empty | `boards.txt` 1283–1291, `platform.txt` 69/79 [V-src] |
| Font metrics, ink-top offset 1, digit advances, flash sizes, `GetStrWidth` last-glyph quirk | Host program against the installed U8g2 2.36.19 clib [V-render] |
| `haxrcorp4089_t_cyrillic` lacks U+044D, U+0401, U+0451; has U+0419 | `u8g2_IsGlyph` [V-render] |
| Every layout in §3.8 as first written (before review round 2) | 53 states rendered and inspected at 4× [V-render] |
| U8g2 API names: `u8g2_UpdateDisplayArea`, `u8x8_cad_StartTransfer/SendCmd/EndTransfer`, `u8x8_SetI2CAddress`, `u8g2_WriteBufferPBM`; noname driver reset delays 100/100 ms | `clib/u8g2.h`, `u8x8.h`, `u8x8_d_ssd1306_128x64_noname.c`, `u8x8_display.c` [V-src] |
| Key rows on the module photo | Measured by me on `oled-0.96-4buttons_listing.png` (cropped and enlarged) |
| **Review round 2** (same day): `u8x8_SetupDefaults` sets `i2c_address = 255` and `CAD_INIT` turns 255 into 0x78, so the I2C address must be set after setup; `SET_FLIP_MODE` sends `0xA0/0xC0` vs `0xA1/0xC8` | `clib/u8x8_setup.c` 95–107, `u8x8_cad.c` 520/566, `u8x8_d_ssd1306_128x64_noname.c` 222–236, 277–288 [V-src] |
| HWCDC: TX ring 256 B created in `begin()` only if none exists; `setTxBufferSize()` before `begin()`; connected `write()` with timeout 0 returns a short count after 20 failed sends; not-connected path is drop-oldest | `cores/esp32/HWCDC.cpp` 293–345, 410–425, 505–600 [V-src] |
| With `CDCOnBoot` off, `Serial` = `Serial0` (UART); `HWCDCSerial` exists only if `ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT`; both macros come from `-D` flags of the board menus | `HardwareSerial.h` 438–452, `HWCDC.h` 109, `platform.txt` 79, `boards.txt` 1203–1204, 1284–1291 [V-src] |
| IDE "Compiler warnings: None" = `-w`, and `-w` hides `#warning` | `platform.txt` 39–40 [V-src]; `g++ -w` on a one-line `#warning` file prints nothing, without `-w` it warns [V-meas] |
| `esp_reset_reason_t` = 16 values (`UNKNOWN`…`CPU_LOCKUP`); wake-cause enum | `esp32s3-libs/3.3.12/include/esp_system/include/esp_system.h` 25–40, `esp_hw_support/include/esp_sleep.h` 108–125 [V-src] |
| Toast/card/list/picker/compass/timer/lap string widths; `_tr` fonts lack `·`, `−`, `°`; Home top-line budget; Cyrillic candidates for Label/Value (cap heights, «э/ё» coverage, widths) | measurement tool against the installed U8g2 2.36.19 clib (`/mnt/wd/arduino-esp32/tmp/spec_fix/`) [V-meas]; the reviewer's numbers (146/118/114/110 px toasts, 114/117 px picker values) reproduced exactly |
| **Not re-rendered in round 2**: the changed layouts (§3.4 budgets, Timer right column, Done overtime, laps, Compass marker/no-sensor column, Details rows, cards). Their numbers are arithmetic from measured widths; WP8/WP9 renders + `make audit` are the check | — |
| **Not verified here**: register maps and timings of the sensors (taken from HW_FACTS: SHT4x VERIFIED by an adversarial verifier, the rest SINGLE-SOURCE), all current figures, RC drift, every [HW?] item | — |
