# Hardware & platform facts — Hiking Gadget v2

Generated 2026-10-11 from the `hiking-gadget-v2-hwfacts` workflow journal (run wf_74eaf859-4dc), no manual edits.

**Status legend**
- `VERIFIED`: an independent adversarial verifier confirmed it (SHT4x domain only).
- `CORRECTED`: the verifier refuted the claim; use the correction.
- `UNCERTAIN`: the verifier could not confirm it independently.
- `SINGLE-SOURCE`: the researcher found it in a source (datasheet, header, or compile test) but nobody re-checked it. The verify phase was stopped by the user to save usage limits.

Domains **not researched** (stopped before completion): `time-persist` (RTC/NVS/TZ/__DATE__), `ssd1306` (OLED power/contrast). See BRIEF and SPEC for these; confirm them on hardware.

Method `compile-test` / `host-run` means the claim was proven by actually compiling against ESP32 core 3.3.12 or running on the host.


## sht4x

| id | status | claim | method/conf | source |
|---|---|---|---|---|
| F01 | CORRECTED | SHT45 exists only at I2C address 0x44 (SHT45-AD1B/AD1F). In the SHT4x family, 0x45 (B-variants) is offered only for SHT40/SHT41/SHT43, and 0x46 (C-variant) only for SHT40-CD1B. The address is fixed by the part and has no pin. <br>**Verifier:** In DS v7.3, 0x45 is offered only for SHT40 (BD1B/BD1F) and SHT43 (BDCB), not for SHT41. The SHT45 = 0x44-only conclusion still holds. The comment 'ADDR_45 // SHT40/41/43-B...' in t1/sht_core.h repeats the same error. | datasheet/high | Sensirion SHT4x datasheet v7.3 (June 2026), Device Overview p.1, Table 11 nomenclature p.21, Table 12 ordering p.22. https://sensirion.com/resource/datasheet/sht4x -> https://sensirion.com/media/docum |
| F02 | VERIFIED | SHT4x command set (all commands are ONE byte): 0xFD = measure T&RH high repeatability, 0xF6 = medium, 0xE0 = low, 0x89 = read serial number, 0x94 = soft reset (ACK only, no data). Heater commands: 0x39 = 200 mW/1 s, 0x32 = 200 mW/0.1 s, 0x2F = 110 mW/1 s, 0x24 = 110 mW/0.1 s, 0x1E = 20 mW/1 s, 0x15 = 20 mW/0.1 s. Every heater command ends with a high-precision measurement and returns 6 bytes. Heater power values are typical at VDD = 3.3 V. | datasheet/high | SHT4x datasheet v7.3, section 4.5, Table 8 (p.12) |
| F03 | VERIFIED | SHT4x timing (max values measured at -40 °C and 1.08 V): power-up tPU 0.3 typ / 1 ms max. Soft reset tSR 1 ms max (also applies to a general-call reset). Measurement low 1.3/1.6 ms, medium 3.7/4.5 ms, high 6.9/8.3 ms (typ/max, tPU included). Heater on-time is 0.9/1.0/1.1 s for the long pulse and 0.09/0.1/0.11 s for the short pulse; the heater turns off automatically. | datasheet/high | SHT4x datasheet v7.3, section 3.1, Table 5 (p.10) |
| F04 | VERIFIED | The SHT4x does NOT clock-stretch. While it is busy measuring or heating, it NACKs the read header, so the firmware can poll for completion. Measurement data can be read only once: the sensor deletes it after the first acknowledged read header. Every transfer is START...STOP, and a read is ended with NACK+STOP. | datasheet/high | SHT4x datasheet v7.3, section 4.1 (p.11) |
| F05 | VERIFIED | Response format for every measurement and heater command is 6 bytes: [T_MSB, T_LSB, CRC(T), RH_MSB, RH_LSB, CRC(RH)], temperature first. The serial number (0x89) is also 6 bytes: two 16-bit words, each followed by a CRC. serial = (word0<<16) \| word1. The master may stop reading after a 16-bit word if it does not need the CRC. | datasheet/high | SHT4x datasheet v7.3, sections 4.3, 4.4, 4.7 (p.11-13) |
| F06 | VERIFIED | CRC-8 (identical for SHT4x and SHT3x): polynomial 0x31 (x^8+x^5+x^4+1), init 0xFF, no input/output reflection, final XOR 0x00, computed over the 2 data bytes. Datasheet test vector: CRC(0xBE,0xEF) = 0x92. A host run of the snippet's crc8() produced 0x92, plus CRC(0x0000)=0x81 and CRC(0xFFFF)=0xAC. | host-run/high | SHT4x datasheet v7.3 Table 7 (p.11); SHT3x-DIS datasheet v7 Table 20 (p.14); host test /mnt/wd/arduino-esp32/tmp/sht4x/host/test_core.cpp |
| F07 | VERIFIED | SHT4x conversion: T[°C] = -45 + 175*S_T/65535 and RH[%RH] = -6 + 125*S_RH/65535. Raw RH can fall outside 0..100, so the datasheet advises clamping RH to 0..100 %RH. Replacing the divisor 65535 with 65536 adds less than 0.002 %RH / 0.003 °C error. Sensirion's integer forms are T_milli = ((21875*S)>>13) - 45000 and RH_milli = ((15625*S)>>13) - 6000. A host run over all 65536 codes measured a max deviation from the exact formula of 0.0037 °C and 0.0026 %RH. The extra beyond 0.003 °C comes from integer truncation. | host-run/high | SHT4x datasheet v7.3 section 4.6 eq.(1),(2) and N.B./N.B.2 (p.12-13); Sensirion embedded-i2c-sht4x sht4x_i2c.c lines 58-72 (https://github.com/Sensirion/embedded-i2c-sht4x); host test output |
| F08 | VERIFIED | SHT4x heater sequence: (1) the heater turns on and a timer starts, (2) when the timer expires, a high-repeatability measurement starts with the heater still on, (3) the heater turns off, (4) the data becomes readable. There is no heater-off command; the maximum on-time per command is 1 s. Limits: maximum duty cycle 10%, defined as total heater-on time ≤ 10% of the sensor's lifetime. Sensor specifications are invalid while heating. Thermally induced stress can offset the temperature reading. The sensor temperature (ambient plus heater rise) must not exceed 125 °C, and the heater may only be operated at ambient < 65 °C. The heater draws up to ~75 mA; Table 4 gives 60 mA typ / 100 mA max at 200 mW, 33/55 mA at 110 mW and 6/10 mA at 20 mW, so the supply must not sag, or the sensor may reset. | datasheet/high | SHT4x datasheet v7.3 section 4.9 + Table 9 (p.13-14), Table 4 (p.9) |
| F09 | VERIFIED | Sensirion's documented heater use cases are (1) removing condensed or spray water, which leaves the sensor non-responsive to RH, and (2) creep-free operation at high humidity. Creep is a reversible positive RH offset that builds up during long exposure to more than ~90 %RH; the datasheet example is +3 %RH after 60 h at >80 %RH. The recommended normal range is 5..60 °C and 20..80 %RH. | official-docs/high | SHT4x datasheet v7.3 section 4.9 'Possible Heater Use Cases' (p.14) and section 2.3 (p.7); Application Note 'Using the Integrated Heater of SHT4x in High-Humidity Environments' v1 Apr 2022 (https://se |
| F10 | VERIFIED | Heater temperature rise measured by Sensirion at 25 °C base, on a 1.5 mm FR-4 PCB with the die pad not soldered: 0x15 +3 °C, 0x1E +6 °C, 0x24 +16 °C, 0x2F +31 °C, 0x32 +29 °C, 0x39 +50 °C (average ΔT). Sensirion notes that the values depend strongly on the board and on airflow. | official-docs/high | Creep Mitigation AN v1 (Apr 2022), Table 1, p.3 |
| F11 | VERIFIED | Sensirion's recommended heater schemes. Continuous: each cycle is heat pulse, then wait for thermal equilibrium, then read. A 200 mW / 1 s pulse every minute with 1 min equilibration almost fully removed creep at 90 %RH. Cooldown can be judged by comparing T with the value just before heating, and it is longer below 25 °C. Single-shot after unknown exposure: 200 mW pulses every 2 s for 2 minutes, then ~2 minutes of cooling before a valid reading. The SEN6x note (SHT4x inside a module) says to wait at least 20 s after a 200 mW / 1 s activation before taking a new measurement. | official-docs/high | Creep Mitigation AN v1 sections 3-4 (p.3-4); SEN6x AN 'Using the Integrated Heater of SHT4x' v1.0 Jan 2026 section 3.1 (https://sensirion.com/media/documents/EA96615B/69709F91/PS_AN_SEN6x_Using_the_In |
| F12 | VERIFIED | SHT4x supply current with the heater off: idle 0.08 µA typ / 1.0 µA max at 25 °C (3.4 µA max at 125 °C), power-up 50 µA, measuring 320 µA typ / 500 µA max. At 1 measurement per second the average is 2.2 µA (high), 1.2 µA (medium) or 0.4 µA (low repeatability). VDD range is 1.08..3.6 V with 3.3 V typical. | datasheet/high | SHT4x datasheet v7.3 Table 4 (p.9) |
| F13 | VERIFIED | SHT4x reset options: soft reset 0x94, general-call reset (byte 0x06 sent to address 0x00, which resets EVERY general-call-capable device on the bus), or power-down. A soft reset or general-call reset aborts any running action, including a heater pulse. | datasheet/high | SHT4x datasheet v7.3 section 4.8 (p.13) |
| F14 | VERIFIED | SHT3x single-shot commands are 16-bit, MSB first. Without clock stretching: 0x2400 high, 0x240B medium, 0x2416 low. With clock stretching: 0x2C06, 0x2C0D, 0x2C10. In no-stretch mode the sensor NACKs the read header until data is ready. Reply is 6 bytes [T_MSB T_LSB CRC RH_MSB RH_LSB CRC]. At least 1 ms is required between commands. Addresses: 0x44 with ADDR low (default), 0x45 with ADDR high. Each 16-bit command includes a 3-bit CRC. | datasheet/high | SHT3x-DIS datasheet v7 (Dec 2022) sections 3.4, 4, 4.3-4.4, Table 9 (p.9-10). https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf |
| F15 | VERIFIED | SHT3x timing at 2.4..5.5 V: measurement low 2.5/4 ms, medium 4.5/6 ms, high 12.5/15 ms (typ/max). Below 2.4 V the maxima rise to 4.5/6.5/15.5 ms. Power-up is 0.5/1 ms (1.5 ms max below 2.4 V); soft reset is 0.5/1.5 ms. Conversion: T = -45 + 175*S/65535 and RH = 100*S/65535; the RH formula has no offset and is never clamped. Sensirion's integer forms are T_milli = ((21875*S)>>13)-45000 and RH_milli = (12500*S)>>13. VDD range is 2.15..5.5 V. | datasheet/high | SHT3x-DIS datasheet v7 Tables 3,4,5 (p.6-7), section 4.13 (p.14); Sensirion embedded-i2c-sht3x sht3x_i2c.c lines 54-60 |
| F16 | VERIFIED | Other SHT3x commands: soft reset 0x30A2, break/stop periodic 0x3093 (takes 1 ms), heater on 0x306D / off 0x3066 (intended for plausibility checks only, a few °C), read status 0xF32D (1 word + CRC), clear status 0x3041, periodic fetch 0xE000. Status bit 1 = 'last command not processed (invalid or failed command CRC)'. Bit 4 = reset detected (default 1). Bit 15 = alert pending (default 1). Bit 13 = heater on. | datasheet/high | SHT3x-DIS datasheet v7 sections 4.6-4.11, Tables 11,13,14,16,17,18,19 (p.11-13) |
| F17 | VERIFIED | SHT3x read-serial command 0x3780 (no clock stretch) returns 2 words + CRC. It is NOT in the SHT3x-DIS datasheet v7; it comes from Sensirion's own driver. | source-or-header/medium | https://github.com/Sensirion/embedded-sht/blob/master/sht3x/sht3x.c line 62 |
| F18 | VERIFIED | Command collision: the first byte of the SHT3x single-shot no-stretch commands (0x2400/0x240B/0x2416) is 0x24, which is the SHT4x command 'heater 110 mW for 0.1 s'. In the same way, the SHT3x fetch command 0xE000 starts with the SHT4x 'measure low' command 0xE0. SHT3x commands with MSB 0x2C, 0x30, 0xF3 or 0x37 (0x2C06, 0x30A2, 0xF32D, 0x3780) do NOT start with any documented SHT4x command byte. | datasheet/high | Comparison of SHT4x DS v7.3 Table 8 vs SHT3x-DIS DS v7 Tables 9/11/14/17 |
| F19 | VERIFIED | Sensirion's own Arduino library probes SHT4x BEFORE SHT3x at the shared address, because an SHT3x probe makes the SHT4x's first reading wrong. In issue #27, after an SHT3x-first autodetect, an SHT40 first read 29.50 °C and then settled to ~21.8 °C. Moving SHT4x first fixed it, and the reporter confirmed that autodetect still worked on an SHT3x afterwards. The library also has an unexplained delay(40) between probe attempts. That this issue comes from the 0x24 collision in F18 is an inference. | source-or-header/high | https://github.com/Sensirion/arduino-sht/blob/master/SHTSensor.cpp lines 368-376, 416; https://github.com/Sensirion/arduino-sht/issues/27 |
| F20 | VERIFIED | Wait times in Sensirion's reference SHT4x driver: 10 ms after 0xFD, 5 ms after 0xF6, 2 ms after 0xE0, 10 ms after 0x89 (serial) and after 0x94 (reset), 1100 ms after 1 s heater commands and 110 ms after 0.1 s heater commands. The datasheet gives no wait time for 0x89. Note that 1100 ms is below the worst case of tHeater max 1.1 s + tMEAS,h max 8.3 ms, so poll for ACK or wait ≥1110 ms. The Linux kernel driver also uses 1100/110 ms and enforces a minimum poll interval of 2000 ms. | source-or-header/high | https://github.com/Sensirion/embedded-i2c-sht4x/blob/master/sht4x_i2c.c lines 233,255,277,300,323,436,456; https://github.com/torvalds/linux/blob/master/drivers/hwmon/sht4x.c lines 22,260-289 |
| F21 | VERIFIED | Installed core 3.3.12 bundles ESP-IDF v5.5.5, so Wire uses esp32-hal-i2c-ng.c, which is built on the new i2c_master driver. Wire.endTransmission() with ZERO data bytes runs i2c_master_probe and returns 2 when the address is NACKed (ESP_ERR_NOT_FOUND). Wire.endTransmission() WITH data bytes returns 4 on any NACK, address or data, because the NACK becomes ESP_ERR_INVALID_STATE, which Wire maps to 'other error'; it never returns 3. Wire.requestFrom() returns 0 when the read header is NACKed. The default Wire timeout is 50 ms. IDF logs the NACK only at DEBUG level (compiled out because CONFIG_LOG_MAXIMUM_LEVEL=1). Arduino's log_e('i2c_master_transmit failed') appears only with Core Debug Level ≥ Error. This was established by reading the source only, not tested on hardware. | source-or-header/medium | /mnt/wd/arduino-esp32/data/packages/esp32/tools/esp32s3-libs/3.3.12/versions.txt; .../hardware/esp32/3.3.12/cores/esp32/esp32-hal-i2c.c:19; esp32-hal-i2c-ng.c:259-276,319-323; libraries/Wire/src/Wire. |
| F22 | VERIFIED | The probe + measure + heater-scheduler sketch (sht_core.h + t1.ino) compiles cleanly with --warnings all for esp32:esp32:esp32s3:CDCOnBoot=cdc on core 3.3.12 (Sketch uses 322250 bytes, globals 23776 bytes). It has not been run on hardware. | compile-test/high | /mnt/wd/arduino-esp32/tmp/sht4x/t1/ (t1.ino, sht_core.h); log /mnt/wd/arduino-esp32/tmp/sht4x/compile.log; build /mnt/wd/arduino-esp32/build/hwfacts-sht4x |
| F23 | VERIFIED | SHT45 specs: RH accuracy ±1.0 %RH typ; T accuracy ±0.1 °C typ. RH repeatability is 0.08/0.15/0.25 %RH and T repeatability 0.04/0.07/0.1 °C for high/medium/low. Resolution is 0.01 %RH / 0.01 °C. Response time τ63 is 4 s for RH (1 m/s airflow) and 2 s for T. RH hysteresis is 0.8 %RH. | datasheet/high | SHT4x datasheet v7.3 Tables 1 and 2 (p.4, p.6) |

**Pitfalls**

- NEVER send an SHT3x 0x24xx single-shot command before SHT4x is ruled out. Byte 0x24 is the SHT4x 110 mW / 0.1 s heater command and corrupts the next readings by several °C (arduino-sht issue #27). Probe order: address ping -> SHT4x (0x94 reset, then 0x89 serial with CRC check) -> SHT3x via 0xF32D status read with CRC check -> only then use 0x2400.
- Identify the chip only by a CRC-valid reply, never by 'device ACKs 0x44'. The listing says GY-SHT31-D/SHT45, so the chip is unconfirmed. With CRC checks on both words, a random false positive has probability ~1/65536.
- The reading returned by a heater command (0x39/0x32/0x2F/0x24/0x1E/0x15) is measured while the sensor is still hot. Discard it for display; it is useful only as a 'heater works' diagnostic (ΔT vs pre-heat). v1 legacy code (legacy/v1/.../sht45_driver.cpp) forced a normal read right after the heater, so it published a hot T and a too-low RH.
- After a heater pulse, T reads high and RH reads low until thermal equilibrium. Sensirion uses 1 min equilibration (bare sensor, 200 mW/1 s) and ≥20 s (SEN6x module). Keep showing the last pre-heat value and mark the reading as cooling. Resume when T is back near the pre-heat T or a max cooldown has passed. The 0.2 °C band and 120 s cap in the snippet are design choices, not Sensirion numbers.
- Do not block for 1.1 s inside the UI loop as v1 did. Run the heater as a state machine: send the command, then poll with requestFrom() after ≥1110 ms until it stops returning 0. The SHT4x NACKs the read header while busy, and the data can be read only once.
- On core 3.3.12 (IDF 5.5.5), Wire.endTransmission() with payload returns 4 (not 2 or 3) on a NACK, and requestFrom() returns 0 for both 'busy' and 'gone'. Use a per-command deadline (max time + margin) to tell them apart. A presence ping needs endTransmission() with zero bytes, which returns 2 when absent.
- Use the datasheet MAX times, not the typical ones. arduino-sht waits only 4 ms for SHT4x medium repeatability (max is 4.5 ms) and 2 ms for low (max 1.6 ms, fine). Simplest: high repeatability (0xFD) with a ≥9-10 ms wait, then poll.
- Clamp RH to 0..100 for SHT4x only, since its formula has a -6 offset and can go below 0 or above 100. The SHT3x RH formula (100*S/65535) needs no offset or clamp. The temperature formula is identical for both chips.
- Do not use the I2C general-call reset (0x06 to address 0x00) on Wire: it hits every general-call-capable device on the shared bus (magnetometer). Use the device-specific soft reset instead: 0x94 for SHT4x, 0x30A2 for SHT3x.
- Only run the heater at ambient < 65 °C (datasheet); the snippet gates it at < 60 °C. Keep the total heater-on time far below 10% of lifetime: with 1 s pulses the minimum interval is 10 s, and with 0.1 s pulses it is 1 s. Practical interval options of 5-60 min are 0.03-0.3% duty.
- The 200 mW heater draws 60 mA typ / 100 mA max at 3.3 V. If the breakout's LDO/regulator or wiring sags, the SHT4x may reset mid-pulse (datasheet note 5). The firmware must tolerate a missing or NACKed heater result.
- An SHT4x's VDD max is 3.6 V; the SHT3x tolerates up to 5.5 V. A 'GY-SHT31-D'-style board fitted with an SHT45 must be powered from 3.3 V or through its own regulator.
- ESP32 reset during a heater pulse leaves the SHT4x busy for up to ~1.1 s, and it NACKs reads. Sending soft reset 0x94 before probing aborts the pulse, or else retry the probe for ≥1.2 s before reporting 'sensor not found'.
- Detection failure must be shown on screen (project principle 'молчащая программа — дефект'). Distinguish 'nothing ACKs 0x44/0x45' (wiring/power) from 'device ACKs but no valid SHT4x/SHT3x reply' (unknown chip or bus noise).
- Sensirion's driver and the Linux driver wait exactly 1100 ms after a 1 s heater command, but the worst case is 1100 ms tHeater + 8.3 ms measurement. Wait ≥1110 ms and/or poll.

**Verifier: snippet issues**

- Snippet 3 (Sht state machine), real bug: if the heater result cannot be read, the next sample is published while the sensor is still hot. 'Cannot be read' means the read header is still NACKed after 1110+50 ms (e.g. the sensor reset mid-pulse from a supply sag, which the pitfalls themselves predict) or r<0 (CRC error). The state machine goes back to Idle with cooling_ still false and heatEndMs_ not set. startHeat() never updates lastMeasMs_, so the next normal measurement starts within ≤0.9 s and handleSample() publishes it as valid. That hot T and too-low RH is the same failure the pitfalls blame on v1. Fix: set cooling_=true and heatEndMs_=now as soon as the 0x39 write is ACKed, or on every exit from State::Heating, not only after a successful hot read.
- Snippet 3: there is no failure signal. errors_ is counted but has no accessor. There is no sample age or 'stale' flag. out_.valid stays true forever with frozen values if the sensor disappears. If the sensor fails during cooldown, cooling_ never clears, so the heater stops and the display freezes with nothing on screen. This breaks the BRIEF §5 principle 'Молчащая программа — дефект' and CLAUDE.md rule 6. Expose an error/stale state, e.g. lastGoodMs plus a consecutive-error count, for the UI.
- Snippet 2 (probeAt): it does not retry for the warm-reset case that pitfall #12 describes. Whether an SHT4x ACKs or obeys 0x94 while busy is undocumented, and that is listed as unverified. If it does not, then within ≤1.1 s of an MCU reset during a heater pulse the 0x89 read is NACKed and the function returns Chip::None, so the UI shows 'sensor not found' for a working sensor. The pitfall itself says to retry for ≥1.2 s, and the snippet does not.
- Pitfall #1 states as fact that byte 0x24 'corrupts the next readings by several °C (arduino-sht issue #27)'. Issue #27 never mentions 0x24 or the heater command. The maintainer only guessed that 'attempting to communicate with a SHT3x causes the SHT4x to do something unexpected'. F19 correctly calls the link an inference, but the pitfall drops that qualifier. The rule (never send 0x24xx before ruling out SHT4x) is still sound.
- Snippet 3 busy-polling: after readyAfterMs_, tick() calls requestFrom() on every loop() iteration with no rate limit until data arrives or the 50 ms grace ends. That hammers the bus shared with the magnetometer. With Core Debug Level ≥ Error, each NACK also prints 2 log_e lines (i2c_master_receive failed / i2cRead returned Error 259). Poll every 2-5 ms instead.
- Snippet 1 (JSON form): the one-line 'if (rh < 0) rh = 0; if (rh > 100000) rh = 100000;' triggers -Wmisleading-indentation under host g++ -Wall. The BRIEF plans host unit tests with g++, so split it across lines as t1/sht_core.h already does.
- Snippet 2 readWords: the 'got != n' short-read branch is unreachable on core 3.3.12. i2cRead sets readCount=size on ESP_OK and 0 on any error. This is harmless but the comment '-1 = short read' does not describe real behavior on this core.
- Snippet 3 cooldown criterion: returning within ±0.2 °C of the pre-heat T fails if the ambient temperature changes during cooldown (e.g. stepping outdoors). T and RH then stay frozen for the full 120 s cap. It is labelled a design choice, but the UI should show 'cooling' while this happens.
- t1/sht_core.h comment 'ADDR_45 // SHT40/41/43-B...' is wrong: DS v7.3 Table 12 has no SHT41 0x45 part.

**Verifier: missing facts**

- The heater-interval setting is not backed by a validated Sensirion scheme. The schemes Sensirion actually tested are (a) continuous creep mitigation with one 200 mW/1 s pulse EVERY MINUTE plus 1 min equilibration, and (b) single-shot with 200 mW pulses every 2 s for 2 min, then ~2 min cooling (Creep AN v1 §3-4). A single 1 s pulse every 5-60 min has unknown effect on creep or condensation. DS v7.3 §4.9: 'For extended heating periods it is required to send periodic heater-on commands ... To obtain a fast increase in temperature the idle time between consecutive heating pulses shall be kept minimal.' Treat the effectiveness of a 5-60 min interval as unverified.
- Core 3.3.12 receive-side logging: a NACKed requestFrom() produces log_e('i2c_master_receive failed: [259] ESP_ERR_INVALID_STATE') in esp32-hal-i2c-ng.c i2cRead plus log_e('i2cRead returned Error 259') in Wire.cpp, whenever Core Debug Level ≥ Error. The default is None (-DCORE_DEBUG_LEVEL=0 in the build command). The F21 'no log spam' claim covers only the transmit side.
- Core 3.3.12: an IDF transaction timeout (SCL held low, software timeout) also returns ESP_ERR_INVALID_STATE through the sync path (i2c_master.c v5.5.5 ~line 727), so Wire.endTransmission() with payload returns 4, not 5. The IDF logs 'I2C software timeout' / 'I2C hardware timeout detected' with ESP_LOGE, which is compiled in because CONFIG_LOG_MAXIMUM_LEVEL=1. A zero-byte probe uses a fixed 100 kHz timing and its own 20 ms SCL timeout.
- SHT3x periodic/ART commands carry SHT4x heater bytes as their SECOND byte: 0x2024 (→0x24), 0x202F (→0x2F), 0x2032 (→0x32) and 0x2B32 ART (→0x32), from SHT3x DS v7 Tables 10 and 12. Never use SHT3x periodic or ART mode until SHT4x has been ruled out, even though the first bytes 0x20/0x2B are not SHT4x commands.
- The SHT3x heater (0x306D) does NOT switch itself off, unlike SHT4x. It stays on until 0x3066, a reset or power loss, and is meant 'for plausibility checking only', at 3.6–33 mW depending on supply (SHT3x DS v7 §4.10, Table 3). The heater-interval setting should be hidden or disabled when an SHT3x is detected. The snippet's canHeat() already gates on SHT4x.
- SHT3x soft reset 0x30A2 is only specified 'When the system is in idle state' (DS v7 §4.9). Use 0x3093 Break first if the device might be in periodic mode.
- Reset by power-down requires SCL and SDA to be pulled low as well (DS v7.3 §4.8: 'Power down (incl. pulling SCL and SDA low)'). Otherwise the sensor can be back-powered through the bus. This matters if the gadget power-gates the sensor in deep sleep. The SHT4x VDD slew rate is ≤20 V/ms (Table 4).
- DS v7.3 §2.5: 'The above-mentioned specifications hold for the stand-alone sensing element' (refer to the Design-in guide SHT). In this gadget the T reading will include self-heating from the ESP32-S3, OLED and LDO inside the enclosure, and RH error follows from that. This was not measured here and is a likely larger error source than ±0.1 °C.
- SHT45 RH accuracy ±1.0 %RH typ is the 25 °C figure. DS v7.3 Fig. 5 (max) and Fig. 9 (tolerance over T and RH) widen it outside the central range. The SHT45 table also gives 'long-term production 0.5 %RH' (mean ±1σ).
- The SHT3x status register bit 4 'System reset detected' (default 1, cleared by 0x3041) can detect sensor brown-outs or resets. It is a cheap health check when an SHT3x is fitted (DS v7 Table 18).

**Unverified (researcher)**

- What an SHT3x does with a lone 1-byte SHT4x command (0x94, 0x89, 0xFD) followed by STOP is NOT documented. From the datasheet (16-bit commands with built-in CRC; no-stretch mode NACKs reads when no data is present), the expected behavior is: ignore it, then NACK the read. The only empirical evidence is secondary: the arduino-sht issue #27 reporter says SHT4x-first autodetect worked on an SHT3x. Not tested on our hardware.
- What an SHT4x does with unknown command bytes such as 0xF3, 0x2D, or the 0x00 second byte of 0x2400 is NOT documented; it may NACK them or ignore them. The probe does not depend on this because it CRC-checks the replies.
- Not documented: whether an SHT4x ACKs a command write while it is busy (only read-header NACK is specified). The datasheet says a soft reset aborts any action, which implies a 0x94 write is accepted while busy, but this was not tested.
- No datasheet wait time for SHT4x 0x89 (serial). The 10 ms used comes from Sensirion's driver.
- SHT3x read-serial 0x3780 comes from Sensirion's embedded-sht driver and does not appear in the SHT3x-DIS datasheet v7.
- Cooldown after a heater pulse on THIS board (GY breakout in the gadget enclosure) has not been measured. Sensirion's 60 s (bare sensor on test PCB) and 20 s (SEN6x module) are reference points only. The 0.2 °C return band and 120 s cap in the snippet are design choices, not Sensirion values.
- Heater ΔT values (+3..+50 °C) come from Sensirion's test PCB at 25 °C and will differ on the actual breakout.
- Heater power at VDD other than 3.3 V, e.g. behind a breakout LDO, is not specified; Sensirion gives typical values at 3.3 V only.
- The Wire return-code mapping on core 3.3.12 (NACK with payload -> 4, empty ping -> 2, requestFrom -> 0 on read-header NACK) and the absence of IDF NACK log spam were derived from source code, not observed on hardware.
- Whether the user's module actually carries an SHT45 (vs an SHT3x) is unconfirmed (BRIEF). The probe must decide at runtime, and the serial number / detected type should be shown on screen.
- No Sensirion document prescribes how a user-selectable 'heater interval' setting should behave. The recommendation is derived from the datasheet and app notes: Off by default; one 0x39 (200 mW, 1 s) pulse per interval, e.g. 5/10/30/60 min; hide the hot sample; freeze the display during cooldown; skip when ambient ≥ 60-65 °C. An optional 'auto' mode that heats only at RH ≥ ~90 % (the creep threshold) would be a product decision.

**Recommended snippets**

_sht_core.h — command codes, CRC-8, conversions (pure C++, also host-tested: CRC(BEEF)=0x92, max int error 0.0037 °C / 0.0026 %RH)_ (compiled: yes)

```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace sht {
constexpr uint8_t ADDR_44 = 0x44, ADDR_45 = 0x45, ADDR_46 = 0x46;
// SHT4x 8-bit commands; reply = [T_msb T_lsb CRC RH_msb RH_lsb CRC]
constexpr uint8_t SHT4X_MEASURE_HIGH = 0xFD, SHT4X_MEASURE_MEDIUM = 0xF6, SHT4X_MEASURE_LOW = 0xE0;
constexpr uint8_t SHT4X_READ_SERIAL = 0x89, SHT4X_SOFT_RESET = 0x94;
constexpr uint8_t SHT4X_HEAT_200MW_1S = 0x39, SHT4X_HEAT_200MW_100MS = 0x32;
constexpr uint8_t SHT4X_HEAT_110MW_1S = 0x2F, SHT4X_HEAT_110MW_100MS = 0x24; // 0x24 == MSB of SHT3x 0x24xx!
constexpr uint8_t SHT4X_HEAT_20MW_1S = 0x1E, SHT4X_HEAT_20MW_100MS = 0x15;
constexpr uint32_t SHT4X_TMEAS_HIGH_MAX_US = 8300, SHT4X_TMEAS_MED_MAX_US = 4500, SHT4X_TMEAS_LOW_MAX_US = 1600;
constexpr uint32_t SHT4X_TSR_MAX_US = 1000, SHT4X_THEAT_1S_MAX_US = 1100000, SHT4X_THEAT_100MS_MAX_US = 110000;
// SHT3x 16-bit commands (MSB first)
constexpr uint16_t SHT3X_MEAS_HIGH_NOSTRETCH = 0x2400, SHT3X_MEAS_MED_NOSTRETCH = 0x240B, SHT3X_MEAS_LOW_NOSTRETCH = 0x2416;
constexpr uint16_t SHT3X_MEAS_HIGH_STRETCH = 0x2C06, SHT3X_READ_STATUS = 0xF32D, SHT3X_CLEAR_STATUS = 0x3041;
constexpr uint16_t SHT3X_SOFT_RESET = 0x30A2, SHT3X_BREAK = 0x3093, SHT3X_HEATER_ON = 0x306D, SHT3X_HEATER_OFF = 0x3066;
constexpr uint16_t SHT3X_READ_SERIAL = 0x3780; // Sensirion embedded-sht driver, not in DS v7

inline uint8_t crc8(const uint8_t* data, size_t len) {   // poly 0x31, init 0xFF, CRC(BE EF)=0x92
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; ++b) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}
inline bool unpackWords(const uint8_t* buf, size_t words, uint16_t* out) {
  for (size_t i = 0; i < words; ++i) {
    const uint8_t* p = buf + 3 * i;
    if (crc8(p, 2) != p[2]) return false;
    out[i] = (uint16_t)((p[0] << 8) | p[1]);
  }
  return true;
}
inline int32_t tempMilliC(uint16_t t) { return ((21875 * (int32_t)t) >> 13) - 45000; }   // SHT3x+SHT4x
inline int32_t sht4xRhMilli(uint16_t s) { int32_t rh = ((15625 * (int32_t)s) >> 13) - 6000;
  if (rh < 0) rh = 0; if (rh > 100000) rh = 100000; return rh; }
inline int32_t sht3xRhMilli(uint16_t s) { return (12500 * (int32_t)s) >> 13; }
inline float tempC(uint16_t s)   { return -45.0f + 175.0f * (float)s / 65535.0f; }
inline float sht4xRh(uint16_t s) { float rh = -6.0f + 125.0f * (float)s / 65535.0f;
  return rh < 0.0f ? 0.0f : (rh > 100.0f ? 100.0f : rh); }
inline float sht3xRh(uint16_t s) { return 100.0f * (float)s / 65535.0f; }
}  // namespace sht
```

_SHT4x/SHT3x safe probe on ESP32 Wire (core 3.3.12) — SHT4x first, SHT3x via 0xF32D; never 0x24xx before SHT4x ruled out_ (compiled: yes)

```cpp
#include <Wire.h>
#include "sht_core.h"
namespace shtdrv {
enum class Chip : uint8_t { None, SHT4x, SHT3x };
struct Probe { Chip chip = Chip::None; uint8_t addr = 0; uint32_t serial = 0; };

static bool writeBytes(TwoWire& w, uint8_t addr, const uint8_t* b, size_t n) {
  w.beginTransmission(addr); w.write(b, n);
  return w.endTransmission() == 0;   // core 3.3.12: NACK with payload -> 4, so test !=0 only
}
static bool writeCmd8(TwoWire& w, uint8_t a, uint8_t c) { return writeBytes(w, a, &c, 1); }
static bool writeCmd16(TwoWire& w, uint8_t a, uint16_t c) {
  const uint8_t b[2] = {(uint8_t)(c >> 8), (uint8_t)(c & 0xFF)}; return writeBytes(w, a, b, 2); }
// 1 = ok, 0 = read header NACKed (busy or absent), -1 = short read / CRC error
static int readWords(TwoWire& w, uint8_t addr, uint16_t* out, size_t words) {
  uint8_t buf[6]; const size_t n = words * 3;
  const size_t got = w.requestFrom(addr, n);
  if (got == 0) return 0;
  if (got != n) { while (w.available()) w.read(); return -1; }
  for (size_t i = 0; i < n; ++i) buf[i] = (uint8_t)w.read();
  return sht::unpackWords(buf, words, out) ? 1 : -1;
}
static Probe probeAt(TwoWire& w, uint8_t addr) {
  Probe p;
  w.beginTransmission(addr);
  if (w.endTransmission() != 0) return p;            // zero-length ping: 2 = nobody there
  writeCmd8(w, addr, sht::SHT4X_SOFT_RESET);          // aborts a heater pulse left by an MCU reset
  delay(2);                                           // tSR max 1 ms
  if (writeCmd8(w, addr, sht::SHT4X_READ_SERIAL)) {
    delay(10);                                        // DS gives no time; Sensirion driver uses 10 ms
    uint16_t sn[2];
    if (readWords(w, addr, sn, 2) == 1) {
      p.chip = Chip::SHT4x; p.addr = addr; p.serial = ((uint32_t)sn[0] << 16) | sn[1]; return p;
    }
  }
  delay(2);                                           // SHT3x needs >= 1 ms between commands
  if (writeCmd16(w, addr, sht::SHT3X_READ_STATUS)) {  // 0xF3 is not an SHT4x command byte
    delay(2);
    uint16_t st;
    if (readWords(w, addr, &st, 1) == 1) { p.chip = Chip::SHT3x; p.addr = addr; return p; }
  }
  return p;
}
}  // namespace shtdrv

// usage in setup():
//   Wire.begin(8, 7, 100000); delay(2);
//   auto p = shtdrv::probeAt(Wire, sht::ADDR_44);
//   if (p.chip == shtdrv::Chip::None) p = shtdrv::probeAt(Wire, sht::ADDR_45);
```

_Non-blocking measurement + periodic heater pulse with cooldown (part of compiled t1.ino)_ (compiled: yes)

```cpp
struct Reading { float tC = NAN; float rh = NAN; bool valid = false; };
class Sht {
 public:
  void begin(TwoWire& w, const shtdrv::Probe& p) { w_ = &w; p_ = p; state_ = State::Idle; }
  void setHeater(uint32_t intervalMs) { heaterIntervalMs_ = intervalMs; lastHeatMs_ = millis(); } // 0 = off
  const Reading& reading() const { return out_; }
  bool coolingDown() const { return cooling_; }
  float lastHeatedTempC() const { return heatedT_; }
  void tick() {
    if (!w_ || p_.chip == shtdrv::Chip::None) return;
    const uint32_t now = millis();
    switch (state_) {
      case State::Idle:
        if (canHeat(now)) { startHeat(now); break; }
        if (now - lastMeasMs_ >= kPeriodMs) startMeasure(now);
        break;
      case State::Measuring:
      case State::Heating: {
        if (now - startedMs_ < readyAfterMs_) break;
        uint16_t v[2];
        const int r = shtdrv::readWords(*w_, p_.addr, v, 2);
        if (r == 0) { if (now - startedMs_ > readyAfterMs_ + kGraceMs) { errors_++; state_ = State::Idle; } break; }
        if (r < 0) { errors_++; state_ = State::Idle; break; }
        handleSample(now, v[0], v[1]);
        state_ = State::Idle;
        break;
      }
    }
  }
 private:
  enum class State : uint8_t { Idle, Measuring, Heating };
  static constexpr uint32_t kPeriodMs = 2000, kGraceMs = 50;
  static constexpr uint32_t kCoolMinMs = 20000;   // SEN6x AN: >= 20 s after 200 mW/1 s
  static constexpr uint32_t kCoolMaxMs = 120000;  // creep AN used 60 s; cap 120 s (design choice)
  static constexpr int32_t kCoolBandMilliC = 200; // back within 0.2 C of pre-heat T (design choice)
  bool canHeat(uint32_t now) const {
    return p_.chip == shtdrv::Chip::SHT4x && heaterIntervalMs_ && !cooling_ &&
           (now - lastHeatMs_ >= heaterIntervalMs_) && out_.valid && out_.tC < 60.0f; // DS: ambient < 65 C
  }
  void startMeasure(uint32_t now) {
    bool ok;
    if (p_.chip == shtdrv::Chip::SHT4x) { ok = shtdrv::writeCmd8(*w_, p_.addr, sht::SHT4X_MEASURE_HIGH); readyAfterMs_ = 9; }
    else { ok = shtdrv::writeCmd16(*w_, p_.addr, sht::SHT3X_MEAS_HIGH_NOSTRETCH); readyAfterMs_ = 16; }
    lastMeasMs_ = now; startedMs_ = now;
    if (ok) state_ = State::Measuring; else errors_++;
  }
  void startHeat(uint32_t now) {
    preHeatMilliC_ = (int32_t)(out_.tC * 1000.0f);
    if (shtdrv::writeCmd8(*w_, p_.addr, sht::SHT4X_HEAT_200MW_1S)) {
      startedMs_ = now; readyAfterMs_ = 1110;      // tHeater max 1.1 s + tMEAS,h max 8.3 ms
      state_ = State::Heating;
    } else errors_++;
    lastHeatMs_ = now;
  }
  void handleSample(uint32_t now, uint16_t sT, uint16_t sRH) {
    const int32_t tm = sht::tempMilliC(sT);
    if (state_ == State::Heating) { heatedT_ = tm / 1000.0f; cooling_ = true; heatEndMs_ = now; lastMeasMs_ = now; return; }
    if (cooling_) {
      const uint32_t since = now - heatEndMs_;
      const bool back = labs(tm - preHeatMilliC_) <= kCoolBandMilliC;
      if (since < kCoolMinMs || (!back && since < kCoolMaxMs)) return;   // keep pre-heat values on screen
      cooling_ = false;
    }
    out_.tC = tm / 1000.0f;
    out_.rh = (p_.chip == shtdrv::Chip::SHT4x ? sht::sht4xRhMilli(sRH) : sht::sht3xRhMilli(sRH)) / 1000.0f;
    out_.valid = true;
  }
  TwoWire* w_ = nullptr; shtdrv::Probe p_; State state_ = State::Idle; Reading out_;
  uint32_t lastMeasMs_ = 0, startedMs_ = 0, readyAfterMs_ = 0, heaterIntervalMs_ = 0, lastHeatMs_ = 0, heatEndMs_ = 0;
  int32_t preHeatMilliC_ = 0; bool cooling_ = false; float heatedT_ = NAN; uint32_t errors_ = 0;
};
// Full compiled file: /mnt/wd/arduino-esp32/tmp/sht4x/t1/t1.ino (class lives in namespace shtdrv there)
```


## qmc5883l

| id | status | claim | method/conf | source |
|---|---|---|---|---|
| Q1 | SINGLE-SOURCE | I2C 7-bit address is 0x0D (0b0001101), fixed. The 8-bit forms are 0x1A for write and 0x1B for read. Other addresses only by special order from the factory. | datasheet/high | QST QMC5883L datasheet Rev A (doc 13-52-04) §5.4 p.13, https://www.qstcorp.com/upload/pdf/202202/13-52-04%20QMC5883L%20Datasheet%20Rev.%20A(1).pdf; identical in Rev B (QST-PD-B002-22) §5.4, https://ww |
| Q2 | SINGLE-SOURCE | I2C supports standard 100 kHz and fast 400 kHz only (SCL 0-400 kHz). External pull-ups are required. The Wire bus shared with SHT45 must therefore run at 400 kHz or less. | datasheet/high | Datasheet Rev A §5.4 and Table 9 (p.13, p.17); Rev B Table 8 |
| Q3 | SINGLE-SOURCE | Data registers: 0x00 XOUT[7:0], 0x01 XOUT[15:8], 0x02 YOUT L, 0x03 YOUT H, 0x04 ZOUT L, 0x05 ZOUT H. Each axis is a 16-bit two's-complement value stored LSB first (little-endian), in X,Y,Z order. Output saturates at -32768 and 32767. This is NOT the HMC5883L layout, which is MSB first in X,Z,Y order at 0x03-0x08. | datasheet/high | Datasheet Rev A §9.1 Table 13, §9.2.1 Table 14 (p.20); Rev B Table 12/13 |
| Q4 | SINGLE-SOURCE | Status register 0x06 (read only): bit0 DRDY, bit1 OVL, bit2 DOR, bits 7..3 reserved. DRDY=1 when all three axes are loaded into the output registers. Reading any data register 0x00-0x05 clears DRDY to 0. | datasheet/high | Datasheet Rev A §9.2.2 Table 15 (p.21); Rev B Table 14 |
| Q5 | SINGLE-SOURCE | Overflow: OVL (0x06 bit1)=1 if any axis is out of range. That axis value is clamped to -32768/32767. OVL clears by itself when the next measurement is back in range, so no explicit clear is needed. The firmware should treat such a sample as invalid. | datasheet/high | Datasheet Rev A §9.2.2 (p.21); Rev B §9.2.2 |
| Q6 | SINGLE-SOURCE | DOR (0x06 bit2)=1 means a whole sample was overwritten before it was read. The two clearing rules in the datasheet do not match. §9.2.2 says DOR is cleared by reading any data register. §6.2.1.3 says DOR goes to 0 once 0x06 is accessed in the next read. Read 0x06 first and then 0x00-0x05 to satisfy both. | datasheet/high | Datasheet Rev A §6.2.1.3 (p.15) and §9.2.2 (p.21) |
| Q7 | SINGLE-SOURCE | Temperature 0x07 (L) / 0x08 (H) is int16 two's complement at about 100 LSB/degC. Its gain is factory-calibrated but its offset is NOT, so only relative temperature is meaningful. Do not display it as ambient temperature. | datasheet/high | Datasheet Rev A §9.2.3 (p.21), Table 2 (100 LSB/℃) |
| Q8 | SINGLE-SOURCE | Control 1 at 0x09 (R/W), with field values left-shifted into place: - OSR[7:6]: 00=512 (default), 01=256, 10=128, 11=64. - RNG[5:4]: 00=±2 G, 01=±8 G (10 and 11 reserved). - ODR[3:2]: 00=10 Hz, 01=50 Hz, 10=100 Hz, 11=200 Hz. - MODE[1:0]: 00=Standby, 01=Continuous (10 and 11 reserved). Shifted constants: ODR 0x00/0x04/0x08/0x0C; RNG 0x00/0x10; OSR 0x00/0x40/0x80/0xC0; MODE 0x00/0x01. | datasheet/high | Datasheet Rev A §9.2.4 Table 18 (p.22); Rev B Table 16; cross-checked with RIOT drivers/include/qmc5883l.h enums (QMC5883L_ODR_50HZ=(1u<<2), QMC5883L_RNG_8G=(1u<<4), QMC5883L_OSR_64=(3u<<6)) |
| Q9 | SINGLE-SOURCE | The datasheet example value CTRL1=0x1D is OSR 512, ±8 G, 200 Hz, continuous; the compiled static_assert matches it. Other useful values: - 0x01: OSR512, 2 G, 10 Hz, continuous. - 0x05: OSR512, 2 G, 50 Hz. - 0xC1: OSR64, 2 G, 10 Hz (lowest-power continuous). - 0x00: standby. | compile-test/high | Datasheet Rev A §7.1/§7.3 (p.16-17); values computed and checked by static_assert in /mnt/wd/arduino-esp32/tmp/qmc5883l/t2/t2.ino and host test in scratchpad hosttest.cpp |
| Q10 | SINGLE-SOURCE | Control 2 at 0x0A: bit7 SOFT_RST (1 = soft reset, restores default values of all registers, goes to standby), bit6 ROL_PNT (1 = I2C pointer rolls over within 0x00-0x06), bit0 INT_ENB. INT_ENB is INVERTED: 0 = DRDY pin ENABLED, 1 = DRDY pin DISABLED. Access is listed as 'R/W, Read only on blanks'. | datasheet/high | Datasheet Rev A §9.2.4 + Table 19 (p.22), §6.2.1.5 (p.16); Rev B Table 17; v1.0 copy https://nettigo.pl/attachments/440 line 'INT_ENB: "0": enable interrupt PIN, "1": disable interrupt PIN' |
| Q11 | SINGLE-SOURCE | RIOT OS uses the same INT_ENB polarity. qmc5883l_irq_enable() writes CTRL2=0, and qmc5883l_irq_disable() / plain init write CTRL2=QMC5883L_INT_ENB (0x01). The constants are SOFT_RST=0x80, ROL_PNT=0x40, INT_ENB=0x01, DRDY=0x01, OVL=0x02, DOR=0x04. The MCU GPIO is configured for GPIO_RISING. | secondary/high | https://github.com/RIOT-OS/RIOT/blob/master/drivers/qmc5883l/qmc5883l.c and drivers/qmc5883l/qmc5883l_internal.h |
| Q12 | SINGLE-SOURCE | The legacy v1 driver writes CTRL2=0x01 with the comment 'Enable DRDY interrupt'. Per the datasheet this DISABLES the DRDY pin. The v1 100 ms polling fallback would hide the problem, so v1 was likely running in polling mode. This is a code-vs-datasheet finding and was not checked on hardware. | source-or-header/high | /home/serafimmo/projects/coding/Hiking_Gadget/legacy/v1/Hiking_Gadget/src/drivers/compass_driver.cpp:59 vs datasheet §9.2.4 |
| Q13 | SINGLE-SOURCE | SET/RESET period register 0x0B (FBR[7:0]): the recommended value is 0x01. The datasheet setup example writes 0x0B=0x01 BEFORE 0x09. | datasheet/high | Datasheet Rev A §9.2.5 (p.22), §7.1 (p.16); Rev B §9.2.5, §7.1 |
| Q14 | SINGLE-SOURCE | Chip ID register 0x0D is documented to return 0xFF. 0x0C is reserved. PX4 probes using CHIP_ID==0xFF. A 2026 Linux IIO patch only warns on a mismatch because an earlier datasheet rev 1.0 had this register reserved. Treat a 0xFF read as a weak/advisory check. | datasheet/high | Datasheet Rev A §9.2.6 Table 21 (p.22-23); Rev B §9.2.6 Table 19; PX4 src/drivers/magnetometer/qmc5883l/QST_QMC5883L_registers.hpp; LKML https://lkml.rescloud.iu.edu/hypermail/linux/kernel/2606.1/1162 |
| Q15 | SINGLE-SOURCE | The DRDY pin (pin 15) is ACTIVE HIGH and a LEVEL signal, not a pulse. It is low by default, goes high when a new sample is in the output registers, and stays high until a data register is read. The pin output is push-pull (both VOH and VOL are specified), so it needs no pull resistor. | datasheet/high | Datasheet Rev A Table 5 pin 15 (p.6), Table 4 I/O characteristics (p.5); Rev B same |
| Q16 | SINGLE-SOURCE | DRDY timing: - It asserts at the end of each measurement. The normal read is: check DRDY pin or 0x06 bit0, then read 0x00-0x05, which clears DRDY and DOR (Figure 10 marks 'Clear by reading any of the data reg', not the 0x06 read). - If a sample is not read, the next sample overwrites it, DRDY stays high and DOR=1 (Figure 12). No new rising edge occurs. - If data is read during a measurement, no DRDY is set for that measurement; the sample is missed and DOR is set (Figure 11). | datasheet/high | Datasheet Rev A §6.2.1.1-6.2.1.3, Figures 10-12 (p.15-16) |
| Q17 | SINGLE-SOURCE | Data lock: reading any data register locks 0x00-0x05 against updates until 0x05 (ZOUT MSB) has been read. If 0x05 is not read before the next measurement ends, the new sample is blocked and DOR=1. Always burst-read through 0x05. | datasheet/high | Datasheet Rev A §6.2.1.1 item 4 and §6.2.1.4 (p.15-16); ESPHome qmc5883l.cpp comment |
| Q18 | SINGLE-SOURCE | Register auto-increment: after a write phase that sets the register pointer, a repeated START and read phase reads sequential registers. With ROL_PNT=1 the pointer wraps within 0x00-0x06. | datasheet/high | Datasheet Rev A §8.2.4, §8.2.5 (p.18-19) |
| Q19 | SINGLE-SOURCE | Standby is the default after POR and after soft reset. Registers are retained by an internal LDO, any I2C access wakes the interface, and no measurement runs (clock halted). Standby current is 3 µA typical (VDD+VDDIO total). To enter standby, write 0x09=0x00 (or the CTRL1 value with MODE bits cleared). Mode changes have no restrictions. | datasheet/high | Datasheet Rev A §6.2.2 (p.16), §7.3 (p.17), Table 2 (p.4), §9.2.4 |
| Q20 | SINGLE-SOURCE | Continuous-mode current, typical, as low-power (OSR=64) / high-power (OSR=512): 10 Hz 75/100 µA; 50 Hz 150/250 µA; 100 Hz 250/450 µA; 200 Hz 450/850 µA. Peak current during a measurement is 2.6 mA. The datasheet gives NO current figures for OSR 128 or 256. ODR 10 Hz is QST's recommendation for compassing. | datasheet/high | Datasheet Rev A Table 2 (p.4), §9.2.4 (p.21); Rev B Table 2 |
| Q21 | SINGLE-SOURCE | Sensitivity (calibrated at zero field) is 12000 LSB/G at ±2 G and 3000 LSB/G at ±8 G, i.e. 120 and 30 LSB/µT. Other figures: - Field resolution 2 mG (std dev of 100 samples, ±2 G). - Digital resolution 0.1-1.0 mG. - Offset ±10 mG; sensitivity tempco ±0.05 %/°C. - Heading accuracy 1-2°. Computed: raw ±32767 corresponds to about ±2.73 G per axis at RNG=2G and about ±10.9 G at RNG=8G. | datasheet/high | Datasheet Rev A Table 2 (p.4), Abstract (p.2); conversion computed by host test |
| Q22 | SINGLE-SOURCE | POR completion time (PORT) is at most 350 µs after VDD and VDDIO are valid. Supply ramp (PSUP) is typically 50 ms and is not controlled by the device. The datasheet gives no separate soft-reset completion time. Existing drivers use these waits: - ESPHome: delay(10) ms. - Linux patch: 350 µs. - PX4: 100 ms, then checks that SOFT_RST has self-cleared. | datasheet/high | Datasheet Rev A §5.3 Table 7 (p.12-13); ESPHome components/qmc5883l/qmc5883l.cpp setup(); PX4 QMC5883L.cpp RunImpl() |
| Q23 | SINGLE-SOURCE | Supply limits: VDD 2.16-3.6 V and VDDIO 1.65-3.6 V; absolute maximum 5.4 V. SDA/SCL VIH max is VDDIO+0.3 V. The magnetic data is offset- and temperature-compensated automatically in continuous mode; gains are factory-calibrated. | datasheet/high | Datasheet Rev A Table 2, Table 3, Table 4 (p.4-5), §6.2.1 (p.15) |
| Q24 | SINGLE-SOURCE | QST's minimal sequence: - Continuous setup: 0x0B=0x01, then 0x09=0x1D. - Measurement: check 0x06[0]=1, then read 0x00-0x05. - Standby: 0x09=0x00. - Soft reset: 0x0A=0x80. | datasheet/high | Datasheet Rev A §7.1-7.4 (p.16-17); Rev B §7.1-7.4 |
| Q25 | SINGLE-SOURCE | PX4 reads 7 bytes from 0x00 to 0x06 in one transfer (data plus status) and drops the sample if OVL is set. ESPHome uses a RISING-edge ISR on DRDY and reads only while the DRDY pin is high. ESPHome writes CTRL2=0, which enables the pin, although its code comment wrongly labels 0 as 'disabled'. ESPHome scales temperature as raw*0.01 °C with no offset. | secondary/high | PX4-Autopilot src/drivers/magnetometer/qmc5883l/QMC5883L.cpp (STATE::READ); esphome/components/qmc5883l/qmc5883l.cpp (setup, loop, read_sensor_) |
| Q26 | SINGLE-SOURCE | Arduino-ESP32 3.3.12 (bundled ESP-IDF 5.5.5) compiles esp32-hal-i2c-ng.c, built on driver/i2c_master.h. This HAL: - enables the ESP internal pull-ups (weak; not a substitute for board pull-ups); - turns a zero-length beginTransmission/endTransmission() into i2c_master_probe(); - makes begin(sda,scl,0) default to 100 kHz; - uses a 50 ms default timeout. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/cores/esp32/esp32-hal-i2c-ng.c lines 19,99,131,259-263; libraries/Wire/src/Wire.h:128; tools/esp32s3-libs/3.3.12/include/esp_common/incl |
| Q27 | SINGLE-SOURCE | In core 3.3.12, Wire.endTransmission(false) does no I/O and always returns 0. The write+read happens later in requestFrom() via i2cWriteReadNonStop(). On NACK or timeout, requestFrom() returns 0, because rxLength is only set to the requested size on ESP_OK. Detect errors by checking requestFrom()'s return value. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/libraries/Wire/src/Wire.cpp:446-468, 517-547; cores/esp32/esp32-hal-i2c-ng.c:370-381 |
| Q28 | SINGLE-SOURCE | GPIO interrupt API in core 3.3.12: - attachInterrupt(uint8_t pin, void(*)(void), int mode) - attachInterruptArg(uint8_t pin, void(*)(void*), void* arg, int mode) - Modes include RISING=0x01 and ONHIGH=0x05. A sketch using attachInterruptArg(12, isr, this, RISING) compiled for esp32s3. | compile-test/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/cores/esp32/esp32-hal-gpio.h:60,64,78-79; compile of /mnt/wd/arduino-esp32/tmp/qmc5883l/t2 |
| Q29 | SINGLE-SOURCE | On ESP32-S3, GPIO12 (the DRDY pin) is RTC IO channel 12, so it can also serve RTC-domain wake sources. It is not a strapping pin. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/tools/esp32s3-libs/3.3.12/include/soc/esp32s3/include/soc/rtc_io_channel.h:46; soc_caps.h SOC_RTCIO_PIN_COUNT 22 |
| Q30 | SINGLE-SOURCE | HMC5883L facts for auto-detect (it may be on a board silkscreened HMC5883L): - Address 0x1E. - ID registers 10/11/12 = 'H','4','3' (0x48, 0x34, 0x33). - Data at 0x03-0x08, MSB first, in X,Z,Y order. - DRDY is ACTIVE-LOW: internally pulled high and pulses low for 250 µs when data is ready. This is the opposite polarity and shape from the QMC5883L's active-high level. | datasheet/high | Honeywell HMC5883L datasheet Form #900405 Rev E, Table 2 register list, Tables 18-20, pin 15 description; https://cdn-shop.adafruit.com/datasheets/HMC5883L_3-Axis_Digital_Compass_IC.pdf |
| Q31 | SINGLE-SOURCE | QMC5883P (the third chip sold as GY-273): I2C address 0x2C, chip ID register 0x00 = 0x80. Its layout differs from the QMC5883L: data at 0x01-0x06, status at 0x09, CTRL1 at 0x0A, CTRL2 at 0x0B. This was taken from the PX4 driver, not a QST datasheet (the separate qmc5883p fact-finder covers it). | secondary/medium | PX4-Autopilot src/drivers/magnetometer/qmc5883p/QST_QMC5883P_registers.hpp |
| Q32 | SINGLE-SOURCE | There is reportedly an undocumented 'A' silicon marked 'DA 5883'. It answers at 0x1E with an HMC5883L-compatible map but a broken status register. The documented 'B' silicon is marked 'DB 5833' and answers at 0x0D. | secondary/medium | Paparazzi UAV docs, module mag_qmc5883l, https://docs.paparazziuav.org/latest/module__mag_qmc5883l.html |
| Q33 | SINGLE-SOURCE | What the project's GY-273 listing photo shows (gy-273_compass_listing.png (listing photo, not in the public repo), zoomed): a 5-hole header; two SMD resistors marked '472' (4.7 kΩ, most likely the SDA/SCL pull-ups); a 3-pin SOT-23 part (most likely the 3.3 V LDO; marking unreadable); two larger caps near the chip, consistent with the datasheet's C1 reservoir and S/R capacitor; silkscreen 'HMC5883L' with axis arrows X pointing left, Y pointing down, Z out of the board. The rail the pull-ups connect to cannot be seen. | secondary/medium | gy-273_compass_listing.png (listing photo, not in the public repo) (crop saved at /mnt/wd/arduino-esp32/tmp/qmc5883l/board_zoom.png) |
| Q34 | SINGLE-SOURCE | GY-273 header order is VCC, GND, SCL, SDA, DRDY. The board has an onboard 3.3 V regulator and I2C pull-ups and accepts a 3.3-5 V supply. These are vendor/secondary claims, not from a schematic. | secondary/medium | ShillehTek GY-273 manual https://shillehtek.com/blogs/shillehtek-product-manuals/hmc5883l-gy-273-magnetometer-compass-module-manual |
| Q35 | SINGLE-SOURCE | On the sister board GY-271 (same Honeywell-footprint design family), the SDA/SCL pull-ups are 2.2 kΩ to the INCOMING 5 V supply, with MOSFET level shifters. The board uses a 220 nF S/R capacitor and a 4.7 µF reservoir, and DRDY goes to header pin 5. So pull-up value and rail differ between board variants, and a GY-273 fed from 5 V could put 5 V on SDA/SCL. | secondary/medium | Silicon Chip, Nov 2018, 'El cheapo modules, part 20: two tiny compass modules', https://siliconchip.com.au/Issue/2018/November/El+cheapo+modules%2C+part+20%3A+two+tiny+compass+modules |
| Q36 | SINGLE-SOURCE | Datasheet revisions checked: Rev A (doc 13-52-04, 2016-07-22), a 'v1.0' copy (2016), and Rev B (QST-PD-B002-22; PDF created 2025-04-24, hosted under /202512/). All three agree on the register map, bit meanings, INT_ENB polarity, chip ID 0xFF, 0x0B=0x01, the currents and the sensitivity. The only difference found: Rev B Table 8 gives SCL high period min 0.6 µs where Rev A says 1 µs. | datasheet/high | Local copies: /mnt/wd/arduino-esp32/tmp/qmc5883l/dl/76784d0b.pdf (Rev A), 0fc53196.pdf (nettigo v1.0), revB.pdf (Rev B), with text extracts revA.txt / v10.txt / revB.txt |

**Pitfalls**

- INT_ENB (0x0A bit0) is inverted: writing 1 DISABLES the DRDY pin. Write CTRL2=0x00 to use DRDY on GPIO12. Legacy v1 wrote 0x01 with the comment 'Enable DRDY interrupt' (compass_driver.cpp:59), so per the datasheet its DRDY ISR could never fire, and the 10 Hz polling fallback hid this. Not confirmed on hardware.
- DRDY is a level signal that stays HIGH until 0x00-0x05 is read, not a pulse. A pure RISING-edge ISR can stall for good: if the pin is already high when the ISR is attached, or a flag is consumed without reading the data, no new edge ever comes. Always also check digitalRead(DRDY)==HIGH and keep a timeout poll of about 2-3 ODR periods.
- Always burst-read all of 0x00..0x05 in one transaction. A partial read (e.g. X and Y only) locks the data registers until 0x05 is read; later samples are blocked and DOR is set.
- Read status 0x06 BEFORE the data, because reading any data register clears DRDY (and, per §9.2.2, DOR). OVL does not need clearing: it reflects the latest measurement, so reading it after the data, as PX4 does, also works.
- Reading data registers during a measurement means that measurement never sets DRDY (the sample is lost and DOR=1). Read only after seeing DRDY; do not read on a blind timer.
- On OVL=1 at least one axis is clamped at ±32767, so the heading is wrong. Discard the sample and show the user a message such as 'strong magnet/metal nearby'. The firmware may optionally switch RNG to 8 G. At RNG=2G clamping happens at about 2.73 G per axis. The value that Earth's field is well below this (about 0.25-0.65 G) is general knowledge, not from the datasheet.
- Soft reset (0x0A=0x80) returns the chip to standby and restores the defaults of ALL registers. The datasheet does not document the defaults of 0x0A and 0x0B, so write 0x0B=0x01 and 0x0A explicitly after every reset or POR, then 0x09. Read 0x09 and 0x0B back to confirm. Do not rely on reading back 0x0A ('Read only on blanks').
- Writing 0x09=0x00 (the datasheet standby example) also zeroes ODR, RNG and OSR. Keep the configured CTRL1 in RAM and resume by writing it back, or enter standby with (ctrl1 & ~0x03).
- Before ESP deep sleep, put the QMC5883L into standby (3 µA). In continuous mode it draws 75-850 µA depending on ODR/OSR. Registers are retained in standby, so after wake a single CTRL1 write resumes it, as long as the module stayed powered.
- Arduino-ESP32 3.3.12: Wire.endTransmission(false) always returns 0. An address NACK only shows up as requestFrom() returning 0. Check requestFrom()'s return; never trust endTransmission(false).
- Use a zero-length beginTransmission()/endTransmission() probe (i2c_master_probe) before reading registers at candidate addresses during auto-detect. Reads to an absent address log_e an i2c_master_transmit_receive error when Core Debug Level is set to Error or higher.
- Keep the shared Wire bus (SHT45 + compass) at 400 kHz or below; the QMC5883L does not support Fast-mode Plus.
- Do not hard-fail on chip ID: 0x0D=0xFF is a weak identifier, and the Linux driver author notes that an early datasheet rev had it reserved. A mismatch should be an on-screen warning, not a silent refusal.
- The GY-273 silkscreen says HMC5883L but the chip may be QMC5883L (0x0D), QMC5883P (0x2C), HMC5883L (0x1E) or the undocumented 'DA 5883' (0x1E, HMC-like map, broken status register). Register maps, byte order (QMC LSB-first X,Y,Z vs HMC MSB-first X,Z,Y) and DRDY polarity (QMC active-high level vs HMC active-low 250 µs pulse) all differ. Choose the edge and decoder per detected chip.
- Power the GY-273 from 3V3, not 5 V. The rail of the board's pull-ups is unknown, and the sister board GY-271 pulls SDA/SCL up to the incoming 5 V. ESP32-S3 pins must not see 5 V.
- The ESP internal pull-ups that the Arduino I2C HAL enables are weak. Rely on the board's pull-ups (photo shows 4.7 kΩ '472') for 400 kHz.
- Configure the DRDY pin as plain INPUT, with no pull-up. The QMC drives it push-pull and holds it LOW most of the time, so INPUT_PULLUP would waste current through the ESP pull-up. This is an inference from push-pull output plus mostly-low level; it was not measured.
- The temperature registers 0x07/0x08 have uncalibrated offset. Do not show them as ambient temperature (the SHT45 provides that).
- The datasheet gives current figures only for OSR 64 and 512. Do not quote currents for OSR 128/256.

**Unverified (researcher)**

- Nothing was run on the real board. Every behaviour above comes from datasheets, driver sources and compile tests only (t1/t2/t3 compiled for esp32s3 with core 3.3.12; the LE16 decode and CTRL1 values were tested on the host with g++).
- Which chip is actually on the user's GY-273 (QMC5883L / QMC5883P / HMC5883L / 'DA 5883') is unknown. The listing says QMC5883L and the silkscreen says HMC5883L.
- The GY-273 regulator part number and which rail the '472' pull-ups go to (3.3 V LDO output or VCC) are not determined; the photo cannot show traces. Also unknown: whether the DRDY header pin has any series or pull resistor on the board.
- The exact byte order when ROL_PNT=1 and a read starts at 0x06: whether 0x06 is followed by 0x00..0x05 is inferred from the datasheet wording 'roll between 00H~06H' and not tested.
- Soft-reset completion time is not given in the datasheet. 10 ms (ESPHome) is a margin, not a spec.
- Power-on/reset default values of 0x0A and 0x0B are not documented.
- The state of the DRDY pin after entering standby with an unread sample pending is not documented.
- Time from entering continuous mode to the first DRDY is not specified. About one ODR period is assumed but not verified.
- Whether QMC5883L axis directions match the HMC5883L axis arrows printed on the GY-273 silkscreen (same LGA-16 footprint) is unknown. The datasheet figures could not be reliably compared, so check the heading on hardware by rotating the device.
- Current consumption at OSR 128 and 256 is not specified in any datasheet revision.
- Whether register 0x0D returns 0xFF on every QMC5883L silicon lot: the Linux patch says datasheet rev 1.0 had it reserved, while the v1.0 copy obtained here does document 0xFF.
- QMC5883P register details (Q31) come from the PX4 driver, not a QST datasheet.
- That INPUT_PULLUP on DRDY wastes current (about 3.3 V / ~45 kΩ while DRDY is low) is an inference; it was not measured, and the ESP32-S3 internal pull-up value was not checked in this task.

**Recommended snippets**

_QMC5883L raw-I2C driver: detect, init (soft reset → 0x0B=0x01 → CTRL2=0x00 → CTRL1 → readback), DRDY edge+level+timeout read, OVL/DOR handling, standby/resume (file: /mnt/wd/arduino-esp32/tmp/qmc5883l/t2/t2.ino)_ (compiled: yes)

```cpp
#include <Arduino.h>
#include <Wire.h>

namespace qmc {
constexpr uint8_t ADDR = 0x0D;
constexpr uint8_t REG_XOUT_L = 0x00, REG_STATUS = 0x06, REG_TOUT_L = 0x07;
constexpr uint8_t REG_CTRL1 = 0x09, REG_CTRL2 = 0x0A, REG_SR_PERIOD = 0x0B, REG_CHIP_ID = 0x0D;
constexpr uint8_t CHIP_ID_VAL = 0xFF;
constexpr uint8_t ST_DRDY = 0x01, ST_OVL = 0x02, ST_DOR = 0x04;
constexpr uint8_t MODE_MASK = 0x03, MODE_STANDBY = 0x00, MODE_CONT = 0x01;
constexpr uint8_t ODR_10HZ = 0x00, ODR_50HZ = 0x04, ODR_100HZ = 0x08, ODR_200HZ = 0x0C;
constexpr uint8_t RNG_2G = 0x00, RNG_8G = 0x10;
constexpr uint8_t OSR_512 = 0x00, OSR_256 = 0x40, OSR_128 = 0x80, OSR_64 = 0xC0;
constexpr uint8_t CTRL2_SOFT_RST = 0x80, CTRL2_ROL_PNT = 0x40, CTRL2_INT_DISABLE = 0x01;
constexpr uint8_t SR_PERIOD_RECOMMENDED = 0x01;
static_assert((OSR_512 | RNG_8G | ODR_200HZ | MODE_CONT) == 0x1D, "matches datasheet 7.1 example");
}  // namespace qmc

static bool i2cProbe(TwoWire &w, uint8_t addr) {
  w.beginTransmission(addr);
  return w.endTransmission() == 0;           // zero-length write -> i2c_master_probe() in core 3.3.x
}
static bool i2cRead(TwoWire &w, uint8_t addr, uint8_t reg, uint8_t *buf, size_t n) {
  w.beginTransmission(addr);
  w.write(reg);
  w.endTransmission(false);                  // returns 0 unconditionally when sendStop=false
  if (w.requestFrom(addr, n) != n) return false;   // NACK/timeout surfaces here (returns 0)
  for (size_t i = 0; i < n; i++) buf[i] = (uint8_t)w.read();
  return true;
}
static bool i2cWrite(TwoWire &w, uint8_t addr, uint8_t reg, uint8_t val) {
  w.beginTransmission(addr);
  w.write(reg);
  w.write(val);
  return w.endTransmission() == 0;
}

enum class MagChip : uint8_t { None, QMC5883L, QMC5883P, HMC5883L, Unknown };
static MagChip detectMag(TwoWire &w) {
  uint8_t id[3] = {0, 0, 0};
  if (i2cProbe(w, 0x0D)) return (i2cRead(w, 0x0D, 0x0D, id, 1) && id[0] == 0xFF) ? MagChip::QMC5883L : MagChip::Unknown;
  if (i2cProbe(w, 0x2C)) return (i2cRead(w, 0x2C, 0x00, id, 1) && id[0] == 0x80) ? MagChip::QMC5883P : MagChip::Unknown;
  if (i2cProbe(w, 0x1E)) return (i2cRead(w, 0x1E, 10, id, 3) && id[0] == 'H' && id[1] == '4' && id[2] == '3') ? MagChip::HMC5883L : MagChip::Unknown;
  return MagChip::None;
}

class Qmc5883l {
 public:
  enum class Err : uint8_t { Ok, NoAck, Bus, ConfigMismatch, NoData, Overflow };
  Qmc5883l(TwoWire &w, int drdyPin) : w_(w), drdyPin_(drdyPin) {}

  Err begin(uint8_t ctrl1) {
    using namespace qmc;
    ctrl1_ = ctrl1;
    if (!i2cProbe(w_, ADDR)) return Err::NoAck;
    uint8_t v = 0;
    if (!i2cRead(w_, ADDR, REG_CHIP_ID, &v, 1)) return Err::Bus;
    chipIdOk_ = (v == CHIP_ID_VAL);                     // advisory: do not hard-fail on mismatch
    if (!i2cWrite(w_, ADDR, REG_CTRL2, CTRL2_SOFT_RST)) return Err::Bus;
    delay(10);                                          // DS: POR completion <= 350 us; margin
    if (!i2cWrite(w_, ADDR, REG_SR_PERIOD, SR_PERIOD_RECOMMENDED)) return Err::Bus;
    uint8_t ctrl2 = (drdyPin_ >= 0) ? 0x00 : CTRL2_INT_DISABLE;   // INT_ENB: 0 = pin ENABLED
    if (!i2cWrite(w_, ADDR, REG_CTRL2, ctrl2)) return Err::Bus;
    if (!i2cWrite(w_, ADDR, REG_CTRL1, ctrl1_)) return Err::Bus;
    if (!i2cRead(w_, ADDR, REG_CTRL1, &v, 1)) return Err::Bus;
    if (v != ctrl1_) return Err::ConfigMismatch;
    if (!i2cRead(w_, ADDR, REG_SR_PERIOD, &v, 1)) return Err::Bus;
    if (v != SR_PERIOD_RECOMMENDED) return Err::ConfigMismatch;
    if (drdyPin_ >= 0) {
      pinMode(drdyPin_, INPUT);                         // push-pull output on QMC: no pull needed
      attachInterruptArg(drdyPin_, &Qmc5883l::onDrdy, this, RISING);
    }
    kick();
    return Err::Ok;
  }

  // Non-blocking. Reads when DRDY edge seen, DRDY line is high, or fallback period elapsed.
  Err poll(int16_t &x, int16_t &y, int16_t &z, uint32_t fallbackMs) {
    using namespace qmc;
    bool lineHigh = (drdyPin_ >= 0) && digitalRead(drdyPin_) == HIGH;
    if (!drdyFlag_ && !lineHigh && (millis() - lastMs_ < fallbackMs)) return Err::NoData;
    drdyFlag_ = false;
    lastMs_ = millis();
    uint8_t st = 0;
    if (!i2cRead(w_, ADDR, REG_STATUS, &st, 1)) return Err::Bus;   // status first: reading data clears DRDY/DOR
    if (!(st & ST_DRDY)) return Err::NoData;
    uint8_t b[6];
    if (!i2cRead(w_, ADDR, REG_XOUT_L, b, 6)) return Err::Bus;    // always read through 0x05 (unlocks data)
    x = (int16_t)((uint16_t)b[0] | ((uint16_t)b[1] << 8));
    y = (int16_t)((uint16_t)b[2] | ((uint16_t)b[3] << 8));
    z = (int16_t)((uint16_t)b[4] | ((uint16_t)b[5] << 8));
    dataSkipped_ = (st & ST_DOR) != 0;
    return (st & ST_OVL) ? Err::Overflow : Err::Ok;               // OVL: discard sample, warn user
  }

  bool standby() { return i2cWrite(w_, qmc::ADDR, qmc::REG_CTRL1, (uint8_t)(ctrl1_ & ~qmc::MODE_MASK)); }
  bool resume()  { bool ok = i2cWrite(w_, qmc::ADDR, qmc::REG_CTRL1, ctrl1_); kick(); return ok; }
  bool chipIdOk() const { return chipIdOk_; }
  bool dataSkipped() const { return dataSkipped_; }

 private:
  static void IRAM_ATTR onDrdy(void *arg) { static_cast<Qmc5883l *>(arg)->drdyFlag_ = true; }
  void kick() { lastMs_ = millis(); drdyFlag_ = false; }

  TwoWire &w_;
  int drdyPin_;
  uint8_t ctrl1_ = 0;
  bool chipIdOk_ = false, dataSkipped_ = false;
  volatile bool drdyFlag_ = false;
  uint32_t lastMs_ = 0;
};

static constexpr int PIN_SDA = 8, PIN_SCL = 7, PIN_DRDY = 12;
Qmc5883l mag(Wire, PIN_DRDY);

void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL, 400000);   // QMC5883L: 400 kHz max
  MagChip c = detectMag(Wire);
  Serial.printf("mag chip=%u\n", (unsigned)c);
  if (c == MagChip::QMC5883L) {
    auto e = mag.begin(qmc::OSR_512 | qmc::RNG_2G | qmc::ODR_50HZ | qmc::MODE_CONT);   // 0x05
    Serial.printf("begin=%u chipIdOk=%d\n", (unsigned)e, mag.chipIdOk());
  }
}

void loop() {
  int16_t x, y, z;
  auto e = mag.poll(x, y, z, 60);
  if (e == Qmc5883l::Err::Ok) Serial.printf("%d %d %d%s\n", x, y, z, mag.dataSkipped() ? " (skipped)" : "");
  else if (e == Qmc5883l::Err::Overflow) Serial.println("OVL: field out of range, sample dropped");
  delay(1);
}
```

_Standby before ESP32-S3 deep sleep, resume after wake (file: /mnt/wd/arduino-esp32/tmp/qmc5883l/t3/t3.ino; wake path only — cold boot still needs the full begin() sequence)_ (compiled: yes)

```cpp
#include <Arduino.h>
#include <Wire.h>
#include "esp_sleep.h"

static constexpr uint8_t QMC_ADDR = 0x0D, QMC_REG_CTRL1 = 0x09;
static constexpr int PIN_DRDY = 12;
static uint8_t gQmcCtrl1 = 0x05;   // OSR512 | 2G | 50 Hz | continuous (kept in RAM / NVS)

static bool qmcWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(QMC_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

static void enterDeepSleep(uint64_t us) {
  detachInterrupt(PIN_DRDY);
  bool ok = qmcWrite(QMC_REG_CTRL1, (uint8_t)(gQmcCtrl1 & ~0x03));   // MODE=00 standby, keep other fields
  if (!ok) Serial.println("QMC5883L standby write failed (I2C) - compass may keep drawing ~100-850 uA");
  Serial.flush();
  esp_sleep_enable_timer_wakeup(us);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(8, 7, 400000);
  qmcWrite(QMC_REG_CTRL1, gQmcCtrl1);   // after wake: registers kept in standby, just set MODE back
}

void loop() { delay(5000); enterDeepSleep(10ULL * 1000000ULL); }
```

_Register/bit constants only (drop-in header content; values per datasheet Table 13/15/18/19, cross-checked with RIOT qmc5883l_internal.h)_ (compiled: yes)

```cpp
// QMC5883L @ 0x0D (7-bit)
#define QMC_REG_XOUT_L   0x00  // X L,H Y L,H Z L,H : int16 little-endian
#define QMC_REG_STATUS   0x06  // bit0 DRDY, bit1 OVL, bit2 DOR
#define QMC_REG_TOUT_L   0x07  // int16 LE, ~100 LSB/degC, offset uncalibrated
#define QMC_REG_CTRL1    0x09  // OSR[7:6] RNG[5:4] ODR[3:2] MODE[1:0]
#define QMC_REG_CTRL2    0x0A  // SOFT_RST[7] ROL_PNT[6] INT_ENB[0] (1 = DRDY pin disabled)
#define QMC_REG_SR_PER   0x0B  // write 0x01
#define QMC_REG_CHIPID   0x0D  // reads 0xFF
#define QMC_ST_DRDY 0x01
#define QMC_ST_OVL  0x02
#define QMC_ST_DOR  0x04
#define QMC_MODE_STANDBY 0x00
#define QMC_MODE_CONT    0x01
#define QMC_ODR_10HZ  0x00
#define QMC_ODR_50HZ  0x04
#define QMC_ODR_100HZ 0x08
#define QMC_ODR_200HZ 0x0C
#define QMC_RNG_2G 0x00   // 12000 LSB/G
#define QMC_RNG_8G 0x10   //  3000 LSB/G
#define QMC_OSR_512 0x00
#define QMC_OSR_256 0x40
#define QMC_OSR_128 0x80
#define QMC_OSR_64  0xC0
#define QMC_CTRL2_SOFT_RST 0x80
#define QMC_CTRL2_ROL_PNT  0x40
#define QMC_CTRL2_INT_DIS  0x01
```


## qmc5883p

| id | status | claim | method/conf | source |
|---|---|---|---|---|
| P-ADDR | SINGLE-SOURCE | The QMC5883P 7-bit I2C address is 0x2C, fixed with no address pin. The I2C diagrams show 0x58 for write and 0x59 for read (8-bit). Standard (100 kHz) and fast (400 kHz) modes are supported, and external pull-ups are required. | datasheet/high | QST QMC5883P datasheet Rev E (QST-PD-B002-22), sec 5.4 + Table 10/12, https://www.qstcorp.com/upload/pdf/202512/2C939E5AA0704285BC3BE71132B8629B.pdf (local copy /mnt/wd/arduino-esp32/tmp/qmc5883p/ds/q |
| P-CHIPID | SINGLE-SOURCE | Register 0x00 is CHIPID (read-only) and reads 0x80. The datasheet's own I2C read example reads register 0x00 and gets 0x80. | datasheet/high | QMC5883P datasheet Rev E, sec 9.2.1 and Table 12 |
| P-DATA | SINGLE-SOURCE | The data registers are 0x01..0x06 in the order X_L, X_H, Y_L, Y_H, Z_L, Z_H: LSB first, int16 two's complement, saturating at -32768/+32767. In normal mode the registers refresh at the ODR rate and hold their value until new data arrives. A 6-byte burst read starting at 0x01 is what the Adafruit and ArduPilot drivers do. The datasheet only implies multi-byte reads (master ACK after each byte) and never states auto-increment explicitly. | datasheet/high | QMC5883P datasheet Rev E, Table 14/15, sec 9.2.1, sec 8.2.4; Adafruit_QMC5883P.cpp getRawMagnetic (github.com/adafruit/Adafruit_QMC5883P) |
| P-STATUS | SINGLE-SOURCE | Status register 0x09: bit0 is DRDY and bit1 is OVFL; the other bits are reserved. DRDY is cleared by reading 0x09 itself (on the QMC5883L it is cleared by reading the data registers instead). OVFL sets when any axis exceeds +/-30000 LSB and clears after it is read. Read 0x09 once, keep both bits, then burst-read 0x01..0x06. | datasheet/high | QMC5883P datasheet Rev E, sec 9.2.2 Table 16; sec 7.5 |
| P-CTRL1 | SINGLE-SOURCE | CTRL1 is register 0x0A, read/write. Bits [7:6] OSR2 (00=1, 01=2, 10=4, 11=8, 'down sampling rate'). Bits [5:4] OSR1 (00=8, 01=4, 10=2, 11=1). Bits [3:2] ODR (00=10 Hz, 01=50 Hz, 10=100 Hz, 11=200 Hz). Bits [1:0] MODE (00=Suspend, 01=Normal, 10=Single, 11=Continuous). Field masks: MODE 0x03, ODR 0x0C, OSR1 0x30, OSR2 0xC0. | datasheet/high | QMC5883P datasheet Rev E, Table 17 (sec 9.2.3) |
| P-CTRL2 | SINGLE-SOURCE | CTRL2 is register 0x0B, read/write. Bit7 SOFT_RST (1 = soft reset, restores all register defaults). Bit6 SELF_TEST (auto-clears after the data update). Bits [5:4] are reserved. Bits [3:2] RNG (00=+/-30 G, 01=+/-12 G, 10=+/-8 G, 11=+/-2 G). Bits [1:0] SET/RESET mode (00=set+reset on, 01=set only, 10/11=off). With 'set only' or 'off', the offset is not renewed during measurement. | datasheet/high | QMC5883P datasheet Rev E, Table 18 (sec 9.2.3) |
| P-SENS | SINGLE-SOURCE | Sensitivity by range: +/-30 G = 1000 LSB/G, +/-12 G = 2500 LSB/G, +/-8 G = 3750 LSB/G, +/-2 G = 15000 LSB/G. Sensitivity is calibrated at zero field and drops slightly at high fields. Offset is +/-10 mG. X/Y resolution is 2 mG std dev and Z is 3 mG. Typical heading accuracy is 1-2 degrees. | datasheet/high | QMC5883P datasheet Rev E, Table 2 (sec 2.1) |
| P-REG29 | SINGLE-SOURCE | Register 0x29 is not in the register map (Table 14), and no bit fields are documented. All three setup examples (normal, continuous, self-test) write it first with 0x06, described as 'Define the sign for X Y and Z axis'. Write 0x29=0x06 before configuring. The QMC6310 datasheet has the same undocumented write. The only bit interpretation found is the Core Electronics PiicoDev QMC6310 driver (a different chip): sign = sign_x + 2*sign_y + 4*sign_z, so 0x06 = Y and Z bits set. This mapping is not confirmed for the QMC5883P. | datasheet/high | QMC5883P datasheet Rev E sec 7.1/7.2/7.3; QMC6310 datasheet (LCSC C669299) sec 7; github.com/CoreElectronics/CE-PiicoDev-QMC6310-MicroPython-Module PiicoDev_QMC6310.py lines 20,64,108-109 |
| P-INIT-NORMAL | SINGLE-SOURCE | Official normal-mode setup: 0x29=0x06, then 0x0B=0x08 (set/reset on, range 8 G), then 0x0A=0xCD. 0xCD decodes to OSR2=8, OSR1=8, ODR=200 Hz, MODE=Normal. For a hiking compass, the same sequence with a lower ODR is: 0x0A=0xC5 (50 Hz) or 0xC1 (10 Hz). | datasheet/high | QMC5883P datasheet Rev E, sec 7.1 (decoding per Table 17) |
| P-INIT-CONT | SINGLE-SOURCE | Official continuous-mode setup: 0x29=0x06, 0x0B=0x08, 0x0A=0xC3. 0xC3 decodes to OSR2=8, OSR1=8, MODE=Continuous; ODR bits are ignored in this mode. Continuous mode runs without sleep: maximum ODR is 1500 Hz at 2200 uA typical. This is wrong for a battery gadget; use Normal mode. | datasheet/high | QMC5883P datasheet Rev E, sec 7.2, sec 6.2.3, Table 2 |
| P-CURRENT | SINGLE-SOURCE | Typical currents: Suspend 22 uA. Normal mode at OSR1=1 / OSR1=8: ODR 10 Hz = 35/78 uA, 50 Hz = 85/310 uA, 100 Hz = 150/600 uA, 200 Hz = 280/1180 uA. Continuous mode: 2200 uA. | datasheet/high | QMC5883P datasheet Rev E, Table 2 + Note [1] |
| P-STANDBY | SINGLE-SOURCE | Standby is called Suspend mode: write 0x0A=0x00. It is the default state after POR and after soft reset. Registers are retained, I2C stays active, no measurements run. Suspend must be inserted between any two of Continuous, Single and Normal (Figure 9 state diagram). | datasheet/high | QMC5883P datasheet Rev E, sec 7.4, 6.2.4, 9.2.3, Fig 9 |
| P-SOFTRST | SINGLE-SOURCE | Soft reset: write 0x0B=0x80. All registers return to defaults and the device enters Suspend. No completion time is given. POR completion is at most 250 us. Afterwards, rewrite 0x29, 0x0B and 0x0A. Writing 0x0B with bit7=0 also covers parts where SOFT_RST does not self-clear: the Linux QMC6308 patch, a sibling part, says that bit is not auto-cleared. | datasheet/high | QMC5883P datasheet Rev E sec 7.6, Table 18, Table 7; lkml.iu.edu/2607.2/10472.html (qmc6308_init comment) |
| P-SELFTEST | SINGLE-SOURCE | Self-test works only in Continuous mode. Sequence: 0x29=0x06, 0x0A=0x03, wait for status bit0, read XYZ #1, write 0x0B=0x40, wait 5 ms, read XYZ #2. Deltas are (x1-x2), (y2-y1), (z2-z1). The datasheet gives no pass/fail thresholds. | datasheet/high | QMC5883P datasheet Rev E sec 7.3, 6.2.3 |
| P-NO-DRDY-PIN | SINGLE-SOURCE | The QMC5883P has no DRDY/interrupt pin. The LGA-16 pins are 1 SCK, 2 VDD, 3-8 NC, 9 GND, 10 C1, 11 GND, 12-15 NC, 16 SDA. On the QMC5883L and HMC5883L, pin 15 is DRDY. If the GY-273 carries a QMC5883P, the header's DRDY (GPIO12) gives no data-ready signal: poll status 0x09 bit0. | datasheet/high | QMC5883P datasheet Rev E Table 5 + Fig 3; QMC5883L datasheet Rev B Table 5; HMC5883L datasheet Table 1 |
| P-POWER | SINGLE-SOURCE | VDD is 2.5-3.6 V (absolute max 5.4 V); the single supply also powers I/O. POR completion is at most 250 us. Recommended I2C pull-ups are 2.7k-4.7k. Operating range is -40..105 C in Rev E (Rev A said 85 C). | datasheet/high | QMC5883P datasheet Rev E, Table 2, Table 3, Table 7, sec 4.1 note |
| P-AXES | SINGLE-SOURCE | Axis directions with the marking upright and the pin-1 dot at top-left: QMC5883P +X points left (toward the SCK/VDD pin side), +Y points down (toward pins 5-8), +Z points up. QMC5883L and HMC5883L in the same view: +X points down (toward pins 5-8), +Y points right (toward pins 9-12). On the same footprint the P is therefore rotated 90 degrees from the L/HMC: L_X = P_Y, L_Y = -P_X. Formulas written for the L/HMC give a heading 90 degrees off on a P. | datasheet/medium | QMC5883P datasheet Rev E Fig 2 (rendered page 5 at 200 dpi); QMC5883L Rev B Fig on p.6; HMC5883L datasheet p.4 'TOP VIEW' |
| P-MARKING | SINGLE-SOURCE | QMC5883P package top marking (Rev E): line 1 'SP' (S = supplier code, P = fixed code meaning QMC5883P; Rev A shows 'H' as supplier code, so 'HP'), line 2 '5883', line 3 trace code. The QMC5883L marking line 1 starts with fixed code 'D'. Reading the chip under a magnifier tells P from L without any software. | datasheet/high | QMC5883P datasheet Rev E sec 3.2.2 Fig 4; Rev A sec 3.2.3; QMC5883L Rev B sec 3.2.3 |
| L-ID | SINGLE-SOURCE | QMC5883L: 7-bit address 0x0D. Chip ID register 0x0D reads 0xFF. Data 0x00..0x05 (X,Y,Z, LSB first). Status 0x06 (bit0 DRDY, bit1 OVL, bit2 DOR). CTRL1 0x09: [7:6] OSR 512/256/128/64, [5:4] RNG 2G/8G, [3:2] ODR 10/50/100/200 Hz, [1:0] MODE 00 standby / 01 continuous. CTRL2 0x0A: bit7 SOFT_RST, bit6 ROL_PNT, bit0 INT_ENB. SET/RESET period register 0x0B, recommended value 0x01. Continuous-mode example: 0x0B=0x01, then 0x09=0x1D. Standby: 0x09=0x00. Sensitivity: 12000 LSB/G at 2 G, 3000 LSB/G at 8 G. | datasheet/high | QST QMC5883L datasheet Rev B, sec 5.4, 7.1-7.4, Table 12/16/17, 9.2.5, 9.2.6, Table 2; https://www.qstcorp.com/upload/pdf/202512/13-52-04%20QMC5883L%20Datasheet%20Rev.%20B.pdf |
| L-DRDY | SINGLE-SOURCE | The QMC5883L DRDY pin (pin 15) is active high: low by default, high until a data register is read. INT_ENB is 0x0A bit0, where 0 enables the pin and 1 disables it. Writing 0x0A=0x01 therefore turns the DRDY pin OFF. legacy/v1 compass_driver.cpp does exactly that under the comment 'Enable DRDY interrupt', which is a bug. | datasheet/high | QMC5883L datasheet Rev B, Table 5 pin 15, sec 6.2.1.5, Table 17 text; /home/serafimmo/projects/coding/Hiking_Gadget/legacy/v1/Hiking_Gadget/src/drivers/compass_driver.cpp (writeReg(QMC_REG_CTRL2, 0x01 |
| H-ID | SINGLE-SOURCE | HMC5883L: 7-bit address 0x1E (0x3C write / 0x3D read). Identification registers 0x0A, 0x0B, 0x0C read 0x48 'H', 0x34 '4', 0x33 '3'. Data registers 0x03..0x08 are MSB first in the order X, Z, Y. The DRDY pin is active low: internally pulled high and pulsed low for 250 us. An ISR for it must use FALLING, not RISING. | datasheet/high | Honeywell HMC5883L datasheet, Register List (Table 2), Tables 18-20, Table 1 pin 15, https://cdn-shop.adafruit.com/datasheets/HMC5883L_3-Axis_Digital_Compass_IC.pdf |
| PROBE | SINGLE-SOURCE | Probing to identify the chip: (1) ACK at 0x2C and reg 0x00 == 0x80 means QMC5883P (or a register-compatible QST part). (2) ACK at 0x0D and reg 0x0D == 0xFF means QMC5883L. (3) ACK at 0x1E and regs 0x0A..0x0C == 'H','4','3' means HMC5883L (real Honeywell part). An ACK with a wrong ID must be reported on screen as 'unknown chip at 0xNN, id=..'. The three addresses differ, so probing all of them is harmless. None of them collides with the SHT4x at 0x44 on the same bus. | compile-test/high | Combination of P-ADDR, P-CHIPID, L-ID, H-ID; compiled in /mnt/wd/arduino-esp32/tmp/qmc5883p/t1/t1.ino |
| ID-SHARED | SINGLE-SOURCE | Chip ID 0x80 is not unique to the QMC5883P. The Linux QMC6308 driver patch uses the same layout: ID 0x00=0x80, data at 0x01, status 0x09, CTRL1 0x0A, CTRL2 0x0B, CTRL4 0x29, the same field encodings and the same 1000/2500/3750/15000 LSB/G. The QMC6310 sits at 0x1C/0x3C and uses the same 0x29=0x06 setup. Treat 'ACK at 0x2C + ID 0x80' as QMC5883P-compatible. | secondary/medium | https://lkml.iu.edu/2607.2/10472.html ([PATCH v2 2/2] iio: magnetometer: add support for QST QMC6308); QMC6310 datasheet sec 5.4 |
| WIRE-PROBE | SINGLE-SOURCE | In Arduino-ESP32 3.3.12 (ESP-IDF 5.5, I2C 'ng' driver), Wire.beginTransmission(a); Wire.endTransmission(); with zero payload calls i2c_master_probe(), which is the correct presence test. Wire.endTransmission(false) only defers and always returns 0: a NACK on a register-address write shows up later as requestFrom() returning fewer bytes. Default Wire timeout is 50 ms (setTimeOut). | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/cores/esp32/esp32-hal-i2c-ng.c lines 259-264; .../libraries/Wire/src/Wire.cpp lines 456-466, 517-537; Wire.h line 128 |
| THIRDPARTY-BUGS | SINGLE-SOURCE | Do not copy register values from popular third-party QMC5883P drivers. ArduPilot AP_Compass_QMC5883P.cpp has three errors: it calls write_register(0x06, 0x29) with the arguments swapped; it defines RNG_8G as (0x10<<2) and OSR2_8 as 0x08, which lands in the ODR bits; and it scales at 1000/3000 mG/LSB, the QMC5883L 8 G figure, instead of 3750 LSB/G. The SunFounder fusion_hat driver uses scale 12000 for the P and skips 0x29. Adafruit_QMC5883P begin() only checks the ID and never writes 0x29. | source-or-header/high | https://raw.githubusercontent.com/ArduPilot/ardupilot/master/libraries/AP_Compass/AP_Compass_QMC5883P.cpp lines 56-62,112-118,170; docs.sunfounder.com fusion_hat magnetometer source; github.com/adafru |
| COMPILE | SINGLE-SOURCE | The probe and QMC5883P driver sketch below compiled for esp32:esp32:esp32s3:CDCOnBoot=cdc with Arduino-ESP32 3.3.12: 323018 bytes flash, 23752 bytes RAM. It has not been run on hardware. | compile-test/high | /mnt/wd/arduino-esp32/tmp/qmc5883p/t1/t1.ino; build log /mnt/wd/arduino-esp32/tmp/qmc5883p/compile.log |

**Pitfalls**

- The QMC5883P has no DRDY pin. With a P on the GY-273, GPIO12/DRDY carries no data-ready signal. Do not wait on an interrupt there: poll 0x09 bit0. An unconnected DRDY line may also float, so do not attachInterrupt on it unless the chip is a QMC5883L or HMC5883L, and give it a defined pull.
- DRDY polarity differs by chip. QMC5883L DRDY is active high until data is read (RISING edge). HMC5883L DRDY is an active-low 250 us pulse (FALLING edge).
- QMC5883L CTRL2 bit0 (INT_ENB) is inverted: 1 DISABLES the DRDY pin. legacy/v1 wrote 0x0A=0x01 under the comment 'Enable DRDY interrupt' and so switched the pin off.
- Register maps differ completely. QMC5883P: ID at 0x00, data 0x01-0x06, status 0x09, ctrl 0x0A/0x0B. QMC5883L: data 0x00-0x05, status 0x06, ctrl 0x09/0x0A, ID at 0x0D=0xFF. HMC5883L: data 0x03-0x08 MSB-first in order X,Z,Y. Never reuse a QMC5883L table for the P.
- On the P, DRDY clears when status 0x09 is read (on the L, reading the data clears it). Read 0x09 once per cycle and keep both DRDY and OVFL from that single read; a second read loses them.
- Mode changes on the P must pass through Suspend (write 0x0A=0x00 first). Switching directly from Continuous to Normal is not allowed by the datasheet state diagram.
- Continuous mode (0x0A=0xC3, datasheet example 7.2) draws about 2200 uA. For a battery device use Normal mode (e.g. 0xC5 = 50 Hz, OSR1=8, OSR2=8, about 310 uA, or ODR 10 Hz), and Suspend (0x0A=0x00) when the compass screen is not shown and before deep sleep.
- Soft reset restores defaults, so 0x29 must be rewritten afterwards along with 0x0B and 0x0A. Write 0x0B with bit7=0 after the reset in case SOFT_RST does not self-clear.
- On the same footprint the QMC5883P axes are rotated 90 degrees from the HMC5883L/QMC5883L (per datasheet figures, L_X = P_Y, L_Y = -P_X). The GY-273 silkscreen arrows were drawn for the HMC5883L. Use a per-chip axis remap in config, otherwise the heading is off by 90 degrees.
- In Arduino-ESP32 3.3.x, Wire.endTransmission(false) always returns 0. Detect missing devices with a zero-length endTransmission() probe or by checking that requestFrom() returned the full byte count.
- Chip ID 0x80 is shared with the sibling QST parts QMC6308/QMC6310. Report 0x2C + 0x80 as 'QMC5883P-compatible', not as proof of the exact part.
- Popular third-party QMC5883P drivers carry wrong constants (ArduPilot: swapped 0x29 write, wrong RNG/OSR2 macros, 3000 LSB/G scale; SunFounder: 12000 LSB/G). Take values only from the QST datasheet: 1000/2500/3750/15000 LSB/G.
- OVFL trips at +/-30000 LSB, which is only 2.0 G on the 2 G range. Near magnets or steel, the 8 G range used in the datasheet examples is the safer default; for heading, atan2 does not depend on the scale.
- The board listing is ambiguous (title says QMC5883L, silkscreen says HMC5883L, and P-variants are also sold as GY-273). Auto-detect the chip. If an address ACKs but the ID is wrong, or nothing ACKs, say so on the OLED in plain words (what was found at which address and what to check), not just in a log line.

**Unverified (researcher)**

- None of this has been run on hardware. The user's GY-273 chip (QMC5883L, QMC5883P or a real HMC5883L) is unknown and must be identified by the probe or by reading the chip marking (QMC5883P: 'SP'/'HP' over '5883').
- Register 0x29: the bit meanings are not documented by QST. That bit0/1/2 = X/Y/Z sign comes only from a third-party QMC6310 driver. It is also unknown what polarity results if 0x29 is left at its default, and whether soft reset clears 0x29.
- Whether SOFT_RST self-clears on the QMC5883P (the Linux patch says it does not on the QMC6308), and the soft-reset completion time (the datasheet gives none; 1 ms is used as a guess based on the 250 us POR maximum).
- The effect of OSR2 ('down sampling rate' 1/2/4/8) on the effective output rate is not stated. It is unknown whether ODR 50 Hz with OSR2=8 yields 50 Hz or 50/8 Hz DRDY events. Measure the DRDY rate on hardware.
- Burst-read auto-increment from 0x01 over 6 bytes is not explicitly stated in the QMC5883P datasheet. It is relied on by the Adafruit and ArduPilot drivers but has not been confirmed on this hardware.
- Single-mode conversion time and self-test pass/fail thresholds are not given in the datasheet.
- The axis-orientation conclusion (QMC5883P rotated 90 deg from QMC5883L/HMC5883L on the same footprint) is read from datasheet drawings, not tested. Whether the GY-273 silkscreen arrows match the chip axes for each chip variant is also unchecked.
- GY-273 board level: where the header DRDY pin connects on a QMC5883P board (floating, NC or tied); on-board LDO and pull-up values; whether a QMC5883P board leaves GPIO12 floating.
- Supply-voltage difference: QST Rev E gives 2.5-3.6 V for the QMC5883P, while some distributor pages give 2.16-3.6 V. Only the QST figure is reported here.
- Other QMC5883P I2C addresses may exist ('contact factory'). Only 0x2C is documented.
- The QMC6308 I2C address was not checked. Its sharing of chip ID 0x80 and register layout comes only from a Linux patch under review, not from a QST datasheet.

**Recommended snippets**

_GY-273 chip probe (QMC5883P / QMC5883L / HMC5883L) + QMC5883P raw-Wire driver (init per datasheet 7.1, suspend, DRDY-poll read)_ (compiled: yes)

```cpp
// Sources: QST QMC5883P datasheet Rev E (QST-PD-B002-22), QST QMC5883L datasheet Rev B,
//          Honeywell HMC5883L datasheet.
#include <Arduino.h>
#include <Wire.h>

static TwoWire &bus = Wire;           // sensor bus: SDA=8, SCL=7
static constexpr int PIN_SDA = 8, PIN_SCL = 7;

namespace qmc5883p {
constexpr uint8_t ADDR        = 0x2C;  // "The default I2C address for QMC5883P is 2CH."
constexpr uint8_t REG_CHIPID  = 0x00;  // reads 0x80
constexpr uint8_t CHIPID      = 0x80;
constexpr uint8_t REG_XOUT_L  = 0x01;  // X L,H  Y L,H  Z L,H  (0x01..0x06), int16 two's complement
constexpr uint8_t REG_STATUS  = 0x09;  // bit0 DRDY, bit1 OVFL; both cleared by reading 0x09
constexpr uint8_t REG_CTRL1   = 0x0A;  // [7:6]OSR2 [5:4]OSR1 [3:2]ODR [1:0]MODE
constexpr uint8_t REG_CTRL2   = 0x0B;  // [7]SOFT_RST [6]SELF_TEST [3:2]RNG [1:0]SET/RESET mode
constexpr uint8_t REG_SIGN    = 0x29;  // not in register map; app examples write 0x06 ("sign for X Y Z")
constexpr uint8_t SIGN_VALUE  = 0x06;
constexpr uint8_t ST_DRDY = 0x01, ST_OVFL = 0x02;
constexpr uint8_t MODE_SUSPEND = 0x00, MODE_NORMAL = 0x01, MODE_SINGLE = 0x02, MODE_CONT = 0x03;
constexpr uint8_t ODR_10HZ = 0 << 2, ODR_50HZ = 1 << 2, ODR_100HZ = 2 << 2, ODR_200HZ = 3 << 2;
constexpr uint8_t OSR1_8 = 0 << 4, OSR1_4 = 1 << 4, OSR1_2 = 2 << 4, OSR1_1 = 3 << 4;
constexpr uint8_t OSR2_1 = 0 << 6, OSR2_2 = 1 << 6, OSR2_4 = 2 << 6, OSR2_8 = 3 << 6;
constexpr uint8_t SR_ON = 0x00, SR_SET_ONLY = 0x01, SR_OFF = 0x02;
constexpr uint8_t RNG_30G = 0 << 2, RNG_12G = 1 << 2, RNG_8G = 2 << 2, RNG_2G = 3 << 2;
constexpr uint8_t SELF_TEST = 0x40, SOFT_RST = 0x80;
constexpr float LSB_PER_GAUSS[4] = {1000.0f, 2500.0f, 3750.0f, 15000.0f};  // index = RNG<1:0>
}  // namespace qmc5883p

namespace qmc5883l { constexpr uint8_t ADDR = 0x0D, REG_CHIPID = 0x0D, CHIPID = 0xFF; }
namespace hmc5883l { constexpr uint8_t ADDR = 0x1E, REG_IDA = 0x0A; }  // 0x0A..0x0C = 'H','4','3'

enum class MagChip : uint8_t { None, QMC5883P, QMC5883L, HMC5883L, UnknownAtKnownAddr };

static bool i2cPresent(uint8_t addr) {
  bus.beginTransmission(addr);
  return bus.endTransmission() == 0;  // zero-length write -> i2c_master_probe() in core 3.3.x
}

static bool readRegs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t n) {
  bus.beginTransmission(addr);
  bus.write(reg);
  bus.endTransmission(false);  // repeated start; NOTE: returns 0 even on NACK (deferred)
  if (bus.requestFrom(addr, n) != n) return false;  // real error shows up here
  for (size_t i = 0; i < n; i++) buf[i] = bus.read();
  return true;
}

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  bus.beginTransmission(addr);
  bus.write(reg);
  bus.write(val);
  return bus.endTransmission() == 0;
}

struct ProbeResult { MagChip chip; uint8_t addr; uint8_t id[3]; };

static ProbeResult probeMagnetometer() {
  ProbeResult r{MagChip::None, 0, {0, 0, 0}};
  if (i2cPresent(qmc5883p::ADDR)) {
    r.addr = qmc5883p::ADDR;
    if (readRegs(qmc5883p::ADDR, qmc5883p::REG_CHIPID, r.id, 1) && r.id[0] == qmc5883p::CHIPID)
      r.chip = MagChip::QMC5883P;
    else
      r.chip = MagChip::UnknownAtKnownAddr;
    return r;
  }
  if (i2cPresent(qmc5883l::ADDR)) {
    r.addr = qmc5883l::ADDR;
    if (readRegs(qmc5883l::ADDR, qmc5883l::REG_CHIPID, r.id, 1) && r.id[0] == qmc5883l::CHIPID)
      r.chip = MagChip::QMC5883L;
    else
      r.chip = MagChip::UnknownAtKnownAddr;
    return r;
  }
  if (i2cPresent(hmc5883l::ADDR)) {
    r.addr = hmc5883l::ADDR;
    if (readRegs(hmc5883l::ADDR, hmc5883l::REG_IDA, r.id, 3) && r.id[0] == 'H' && r.id[1] == '4' &&
        r.id[2] == '3')
      r.chip = MagChip::HMC5883L;
    else
      r.chip = MagChip::UnknownAtKnownAddr;
    return r;
  }
  return r;
}

// QMC5883P init per datasheet 7.1/7.2 (sign reg 0x29=0x06, CTRL2 = S/R on + 8 G, then CTRL1)
static bool qmc5883pBegin(uint8_t ctrl1, uint8_t rng = qmc5883p::RNG_8G) {
  using namespace qmc5883p;
  if (!writeReg(ADDR, REG_CTRL2, SOFT_RST)) return false;
  delay(1);  // datasheet gives no soft-reset time; POR completion is max 250 us
  uint8_t id = 0;
  if (!readRegs(ADDR, REG_CHIPID, &id, 1) || id != CHIPID) return false;
  bool ok = writeReg(ADDR, REG_SIGN, SIGN_VALUE);
  ok &= writeReg(ADDR, REG_CTRL2, (uint8_t)(rng | SR_ON));  // also writes SOFT_RST back to 0
  ok &= writeReg(ADDR, REG_CTRL1, MODE_SUSPEND);            // go via suspend between modes
  ok &= writeReg(ADDR, REG_CTRL1, ctrl1);
  return ok;
}

static bool qmc5883pSuspend() { return writeReg(qmc5883p::ADDR, qmc5883p::REG_CTRL1, qmc5883p::MODE_SUSPEND); }

// Returns: 1 = new sample in x/y/z, 0 = no new data, -1 = I2C error. *ovfl set if OVFL flag was seen.
static int qmc5883pRead(int16_t &x, int16_t &y, int16_t &z, bool *ovfl) {
  using namespace qmc5883p;
  uint8_t st;
  if (!readRegs(ADDR, REG_STATUS, &st, 1)) return -1;  // reading 0x09 clears DRDY and OVFL
  if (ovfl) *ovfl = st & ST_OVFL;
  if (!(st & ST_DRDY)) return 0;
  uint8_t b[6];
  if (!readRegs(ADDR, REG_XOUT_L, b, 6)) return -1;
  x = (int16_t)((uint16_t)b[1] << 8 | b[0]);
  y = (int16_t)((uint16_t)b[3] << 8 | b[2]);
  z = (int16_t)((uint16_t)b[5] << 8 | b[4]);
  return 1;
}

void setup() {
  Serial.begin(115200);
  bus.begin(PIN_SDA, PIN_SCL, 400000);
  ProbeResult p = probeMagnetometer();
  Serial.printf("mag chip=%u addr=0x%02X id=%02X %02X %02X\n", (unsigned)p.chip, p.addr, p.id[0], p.id[1], p.id[2]);
  if (p.chip == MagChip::QMC5883P) {
    using namespace qmc5883p;
    // Normal mode, ODR 50 Hz, OSR1=8, OSR2=8 (datasheet 7.1 uses 0xCD = same but ODR 200 Hz)
    bool ok = qmc5883pBegin(OSR2_8 | OSR1_8 | ODR_50HZ | MODE_NORMAL);
    Serial.printf("qmc5883p init %s\n", ok ? "ok" : "FAILED");
  }
}

void loop() {
  int16_t x, y, z;
  bool ovfl = false;
  int r = qmc5883pRead(x, y, z, &ovfl);
  if (r == 1) {
    const float k = 1.0f / qmc5883p::LSB_PER_GAUSS[2];  // 8 G range
    Serial.printf("x=%d y=%d z=%d  |B|=%.3f G%s\n", x, y, z,
                  sqrtf((float)x * x + (float)y * y + (float)z * z) * k, ovfl ? " OVFL" : "");
  }
  static uint32_t t0 = millis();
  if (millis() - t0 > 60000) { qmc5883pSuspend(); while (true) delay(1000); }
  delay(5);
}
```

_Per-chip axis remap into the HMC5883L/QMC5883L frame (from datasheet figures; verify on hardware)_ (compiled: no)

```cpp
// Bring raw chip axes into one board frame before calibration and atan2.
// HMC5883L and QMC5883L share axis directions on the same footprint.
// QMC5883P (datasheet Rev E Fig 2) is rotated 90 deg: L_X = P_Y, L_Y = -P_X, Z same.
struct Vec3i { int32_t x, y, z; };
static Vec3i toBoardFrame(MagChip chip, int16_t rx, int16_t ry, int16_t rz) {
  switch (chip) {
    case MagChip::QMC5883P: return { (int32_t)ry, -(int32_t)rx, (int32_t)rz };
    default:                return { (int32_t)rx, (int32_t)ry, (int32_t)rz };
  }
}
// Then: subtract hard-iron offsets, heading = atan2f(y, x) with a sign/offset that is
// still taken from config, because enclosure mounting is unknown.
```


## hmc5883l

| id | status | claim | method/conf | source |
|---|---|---|---|---|
| F01 | SINGLE-SOURCE | I2C address: 7-bit 0x1E (8-bit 0x3C write / 0x3D read), fixed, no address pin. Bus speed 100 kHz or 400 kHz; Hs mode is not supported. External pull-ups are required. | datasheet/high | Honeywell HMC5883L datasheet, Form #900405 Rev E (Feb 2013), p.11 'Register Access' + p.9 'I2C Interface'. https://cdn-shop.adafruit.com/datasheets/HMC5883L_3-Axis_Digital_Compass_IC.pdf |
| F02 | SINGLE-SOURCE | Register map: 0x00 CRA (R/W), 0x01 CRB (R/W), 0x02 MODE (R/W), 0x03 X MSB, 0x04 X LSB, 0x05 Z MSB, 0x06 Z LSB, 0x07 Y MSB, 0x08 Y LSB, 0x09 STATUS, 0x0A/0x0B/0x0C ID A/B/C (read-only). The table lists the ID registers as decimal 10/11/12, so they are hex 0x0A..0x0C. | datasheet/high | HMC5883L datasheet p.11, Table 2 'Register List' |
| F03 | SINGLE-SOURCE | ID registers: IRA(0x0A)=0x48 'H', IRB(0x0B)=0x34 '4', IRC(0x0C)=0x33 '3'. One 3-byte burst read from 0x0A must return "H43". | datasheet/high | HMC5883L datasheet p.17, Tables 18-20 |
| F04 | SINGLE-SOURCE | Data output order is X, Z, Y (not X, Y, Z). Each axis is big-endian (MSB register first) 16-bit two's complement, with valid range 0xF800..0x07FF (-2048..2047). An ADC overflow or underflow, or a math overflow in bias mode, makes that axis read -4096 (0xF000). | datasheet/high | HMC5883L datasheet p.15 (Data Output X/Y/Z registers) and p.18 (Operational examples) |
| F05 | SINGLE-SOURCE | Address pointer auto-increments after each byte read. To set the pointer, write the register address with no data byte after it. Reading an invalid address returns 0, and writes to invalid addresses or undefined bits are ignored. The datasheet loop re-points to 0x03 before every 6-byte read. | datasheet/high | HMC5883L datasheet p.11 'Register Access', p.18 example loop |
| F06 | SINGLE-SOURCE | CRA (0x00), default 0x10: bit7 must be 0. Bits 6:5 MA1:MA0 set averaging (00=1, 01=2, 10=4, 11=8). Bits 4:2 DO2:DO0 set the continuous output rate (000=0.75, 001=1.5, 010=3, 011=7.5, 100=15 default, 101=30, 110=75 Hz, 111 reserved). Bits 1:0 MS1:MS0 set measurement mode (00 normal, 01 positive bias / self-test, 10 negative bias, 11 reserved). 0x70 = 8 averages, 15 Hz, normal. | datasheet/high | HMC5883L datasheet p.12, Tables 3-6 |
| F07 | SINGLE-SOURCE | CRB (0x01), default 0x20: bits 7:5 GN2:GN0, bits 4:0 must be 0. Gain table (GN: range / LSB per gauss / mG per LSB): 0: ±0.88 Ga / 1370 / 0.73; 1: ±1.3 / 1090 (default) / 0.92; 2: ±1.9 / 820 / 1.22; 3: ±2.5 / 660 / 1.52; 4: ±4.0 / 440 / 2.27; 5: ±4.7 / 390 / 2.56; 6: ±5.6 / 330 / 3.03; 7: ±8.1 / 230 / 4.35. All gains share the output range -2048..2047. The same gain applies to all axes, so XY and Z have no separate LSB/G values. | datasheet/high | HMC5883L datasheet p.13, Tables 7-9 |
| F08 | SINGLE-SOURCE | After a gain change, the first measurement still uses the previous gain. Discard one sample after every CRB write. | datasheet/high | HMC5883L datasheet p.13 |
| F09 | SINGLE-SOURCE | MODE (0x02), default 0x01: MD1:MD0 00 = continuous, 01 = single (power-up default), 10 or 11 = idle. MR7 is labelled 'HS' (enable 3400 kHz I2C), but p.9 says Hs is not supported, so the datasheet contradicts itself. Always write 0 to bits 7..2. Values to use: 0x00 continuous, 0x01 single, 0x02 idle. | datasheet/high | HMC5883L datasheet p.14, Tables 10-12; p.9 |
| F10 | SINGLE-SOURCE | Continuous mode: after power-on or any write to MODE/CRA/CRB, the first full data set is ready after 2/f_DO (133 ms at 15 Hz). After that, a new set arrives every 1/f_DO. Data not read before the next measurement is overwritten. | datasheet/high | HMC5883L datasheet p.14 Table 12; p.10 'Continuous-Measurement Mode' |
| F11 | SINGLE-SOURCE | Single mode: writing MODE=0x01 triggers one measurement. Data is ready about 6 ms later (6.26 ms including the I2C write in the timing chart). The chip then sets RDY and drops to idle by itself. Maximum output rate is 75 Hz in continuous mode and 160 Hz in single mode when it is retriggered on DRDY. | datasheet/high | HMC5883L datasheet p.2 spec table; p.8 'Typical Measurement Period in Single-Measurement Mode'; p.12 |
| F12 | SINGLE-SOURCE | DRDY pin: internally pulled high, active LOW. It pulses low for 250 µs when new data is written to the output registers, so the ESP32 interrupt must be FALLING. Connecting DRDY is optional. The datasheet says monitoring it is only needed for the maximum output rate. The STATUS.RDY bit can be polled instead. | datasheet/high | HMC5883L datasheet p.3 Table 1 (pin 15); p.8 footnote; p.16 Table 17 |
| F13 | SINGLE-SOURCE | STATUS (0x09): bit0 RDY, bit1 LOCK. LOCK is set when some but not all of the six data registers have been read, OR when the MODE register has been read. While LOCK is set, new data is not stored (and DRDY/RDY are affected). LOCK clears when all six bytes are read, MODE is changed, CRA is changed, or power is cycled. So always burst-read all 6 bytes, and never read MODE in the sampling loop. | datasheet/high | HMC5883L datasheet p.16 'Data Output Register Operation', Tables 16-17 |
| F14 | SINGLE-SOURCE | Power: idle mode draws 2 µA typical. Measurement mode draws 100 µA typical, specified only at 7.5 Hz ODR with no averaging (MA=00). VDD is 2.16-3.6 V and VDDIO is 1.71 V to VDD+0.1 V. Turn-on: 200 µs until it accepts I2C commands, 50 ms until the analog circuit is ready. First valid data in the default single mode about 56 ms after power-up. | datasheet/high | HMC5883L datasheet p.2 spec table; p.18 |
| F15 | SINGLE-SOURCE | Datasheet power-on init for continuous mode: CRA=0x70 (8 averages, 15 Hz, normal), CRB=0xA0 (gain 5, 'or any other desired gain'), MODE=0x00. Then wait 6 ms or for DRDY. Loop: read 6 bytes, re-point to 0x03, wait about 67 ms at 15 Hz or for DRDY. For single mode: write MODE=0x01 for each measurement, wait 6 ms or for DRDY, then read 6 bytes. For a hiking compass I recommend CRA=0x70, CRB=0x20 (±1.3 Ga, 1090 LSB/G, see F17) and MODE=0x00 while the compass screen is open, then MODE=0x02 (idle) otherwise and before sleep. | datasheet/high | HMC5883L datasheet p.18 'Operational Examples' |
| F16 | SINGLE-SOURCE | Self-test: set CRA=0x71 (positive bias), CRB=0xA0 (gain 5, 390 LSB/G), continuous mode. Each axis should read 243..575 LSB (nominal about +452 for X/Y and +421 for Z). At other gains, scale the limits by gain/390 (gain 6: 206..487, gain 7: 143..339). Exit by writing CRA=0x70. The bias field is about 1.16 Ga on X/Y and 1.08 Ga on Z. | datasheet/high | HMC5883L datasheet p.19 'Self Test Operation'; p.2 spec table |
| F17 | SINGLE-SOURCE | Earth's field at Moscow (55.756N 37.617E, ~2026.78, WMM2025): declination D=+12.1° E, inclination I=71.7°, horizontal H=16.64 µT (0.166 G), vertical Z=50.4 µT downward, total F=53.1 µT. For reference: St Petersburg D=+12.2°, I=73.8°; Novosibirsk D=+8.2°, I=74.8°. At gain 1 (1090 LSB/G) the horizontal circle radius is about 181 counts and \|B\| about 579 counts. A ±1.3 Ga range leaves margin for hard-iron offsets. | host-run/high | NOAA NCEI WMM2025.COF (https://www.ncei.noaa.gov/sites/default/files/2024-12/WMM2025COF.zip). Evaluated with my own spherical-harmonic script (scratchpad/scripts/wmm.py), which reproduced all official |
| F18 | SINGLE-SOURCE | Chip axes: in the top-view drawing, X and Y lie in the package plane and the Z symbol (circle with dot) points out of the top face. The arrows show the field direction that gives a positive reading. So with the board flat, face up, and X pointing in the direction of travel, Y points LEFT and Z points UP. Heading clockwise from magnetic north = atan2(Y, X), normalised to 0..360. Corollary: in the northern hemisphere the Z reading is negative (about -550 counts at gain 1 in Moscow). This gives a cheap on-hardware check of orientation. | datasheet/medium | HMC5883L datasheet p.4 pin/axis drawing ('TOP VIEW (looking through)'); host simulation scratchpad/scripts/compass_math_test.cpp |
| F19 | SINGLE-SOURCE | Honeywell AN-203 heading formula, with X forward and Y left: Direction = 90 - atan(x/y) for y>0, 270 - atan(x/y) for y<0, 180 for y=0 & x<0, 0 for y=0 & x>0. This is algebraically the same as atan2(y,x) mapped to 0..360. Prerequisites: the device must be level, there must be no nearby ferrous material, and the declination must be known. | official-docs/high | Honeywell AN-203 'Compass Heading Using Magnetometers' (7/95 rev A), https://cdn-shop.adafruit.com/datasheets/AN203_Compass_Heading_Using_Magnetometers.pdf |
| F20 | SINGLE-SOURCE | Declination is positive east of true north. True heading = magnetic heading + declination (east positive), then wrap to 0..360. Declination changes with time and location, so it should be a user setting (Moscow default about +12°). | official-docs/high | NOAA NCEI, https://www.ngdc.noaa.gov/geomag/declination.shtml |
| F21 | SINGLE-SOURCE | Hard-iron is a fixed additive offset vector: the sensor's zero-field offset plus magnetised material on the PCB or in the enclosure. Soft-iron is distortion induced by the Earth's field. Hard-iron often exceeds the geomagnetic field. If it is not subtracted, the heading 'jams' within a limited range. The raw readings lie on a sphere (2-D: circle) centred at the hard-iron offset. | official-docs/high | Freescale/NXP AN4248 Rev 3 'Implementing a Tilt-Compensated eCompass...', sec. 1 & 5, https://www.artekit.eu/resources/ak-mag3110/doc/AN4248.pdf |
| F22 | SINGLE-SOURCE | Min/max calibration per axis: offset = (max+min)/2; half-range r = (max-min)/2; scale = mean(r over axes)/r_axis. For a flat 2-D compass use X and Y only: avg=(spanX+spanY)/2, sclX=avg/spanX, sclY=avg/spanY. This handles hard-iron plus axis-aligned soft-iron only. A rotated ellipse (cross-axis coupling) needs a matrix fit. Host simulation (Moscow field, gain 1): offsets (+250,-140) counts with scales 1.15/0.88 gave up to 179° raw error and 0.00° after min/max calibration. Adding 8% cross-coupling left a 5.2° residual. | secondary/high | Kris Winer, 'Simple and Effective Magnetometer Calibration' (github.com/kriswiner/MPU6050/wiki/Simple-and-Effective-Magnetometer-Calibration) + host simulation scratchpad/scripts/compass_math_test.cpp |
| F23 | SINGLE-SOURCE | Tilt matters because the 2-axis formula assumes the X/Y plane is horizontal. Tilting it mixes the large vertical field component into X/Y. Peak heading error is about tan(I)×tilt for small tilts. In Moscow (I=71.7°, tan=3.03): 1° tilt gives 3.0°, 2° gives 6.1°, 5° gives 15.4°, 10° gives 32°, 15° gives 54° (worst case over headings, pitch or roll). This board has no accelerometer, so the error cannot be compensated. The UI must ask the user to hold the device level. Smoothing does not fix it. | host-run/high | Infineon AN2272 'PSoC 1 Sensing - Magnetic Compass with Tilt Compensation' p.2; AN4248 p.1; host simulation scratchpad/scripts/compass_math_test.cpp |
| F24 | SINGLE-SOURCE | Never low-pass the heading in degrees directly. Either filter the unit vector (EMA on cos and sin, then atan2), or filter a wrap-aware delta (AN4248 sec 7.2: diff = in - out wrapped to ±180, then out += α·diff, then wrap). Host run of a step 350°→10° with α=0.3: the vector filter goes 356.0, 0.2, 3.2, 5.2 ... while a naive degree EMA sweeps through 248, 176, 126 ... For a time constant τ at variable dt, use α = 1 - exp(-dt/τ). | official-docs/high | AN4248 sec 7.2 'Modulo Arithmetic Low Pass Filter for Angles'; Wikipedia 'Circular mean'; host run |
| F25 | SINGLE-SOURCE | Noise floor: 2 mG at GN=0 with no averaging (spec). From the p.8 graph, 8-sample averaging gives about 1.1-1.5 mG across gains (approximate graph reading). Heading noise ≈ atan(noise/H): with H=166 mG (Moscow) that is about 0.4-0.8° rms. Resolution is therefore not the limiting factor; tilt and calibration are. | datasheet/medium | HMC5883L datasheet p.2 spec table, p.8 'Typical Noise Floor' graph; arithmetic in host test |
| F26 | SINGLE-SOURCE | arduino-esp32 3.3.12 Wire: endTransmission(false) does no bus activity. It only sets nonStop=true, keeps the bus lock, and returns 0. The write+read then runs inside requestFrom() via i2cWriteReadNonStop(). NACK or timeout on a register read is therefore only visible as requestFrom() returning fewer bytes than requested. endTransmission() with a stop returns 0 OK, 2 NACK, 5 timeout, 4 other. The default transaction timeout is 50 ms (setTimeOut). begin(int sda, int scl, uint32_t frequency=0) exists. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/libraries/Wire/src/Wire.cpp lines 445-548; Wire.h lines 121, 128 |
| F27 | SINGLE-SOURCE | The recommended driver sketch compiles with --warnings all for esp32:esp32:esp32s3:CDCOnBoot=cdc on core 3.3.12. It covers raw Wire on SDA=8/SCL=7 at 400 kHz, ID check, init, 6-byte X/Z/Y read with -4096 overflow detection, DRDY on GPIO12 with FALLING edge and an IRAM_ATTR ISR, single-shot, self-test, idle, min/max calibration, heading with declination, and a vector LPF. Result: 332158 bytes flash, 24344 bytes RAM. The only warning is an unused hmcSleep(). It has not been run on hardware. | compile-test/high | /mnt/wd/arduino-esp32/tmp/hmc5883l/t1/t1.ino, build path /mnt/wd/arduino-esp32/build/hwfacts-hmc5883l |
| F28 | SINGLE-SOURCE | The Adafruit_HMC5883_Unified library uses 1100 LSB/G for XY and 980 for Z at ±1.3 Ga, plus other per-axis values. These do not match the HMC5883L datasheet, which gives 1090 LSB/G for every axis at GN=1. Do not copy constants from it. Heading (an XY ratio) is unaffected, but \|B\| sanity checks and Z are affected. The library reads bytes in order xhi, xlo, zhi, zlo, yhi, ylo, which agrees with the datasheet. | secondary/high | https://raw.githubusercontent.com/adafruit/Adafruit_HMC5883_Unified/master/Adafruit_HMC5883_U.cpp (setMagGain, read) |
| F29 | SINGLE-SOURCE | Several retail and tutorial sources report that the HMC5883L is discontinued and that most GY-273 boards now carry a QMC5883L at 0x0D, with an incompatible register map. That matches this project's mixed labelling: the listing says QMC5883L, the silkscreen says HMC5883L. Detection must be: ACK at 0x1E AND ID registers = 'H','4','3'. Otherwise probe 0x0D / 0x2C for the QMC variants. | secondary/medium | mischianti.org GY-273 QMC5883L clone article; zbotic.in HMC5883L vs QMC5883L comparison; docs/BRIEF.md hardware table |

**Pitfalls**

- Data registers are ordered X, Z, Y (0x03-0x08), each MSB first. Code that maps them as X, Y, Z gives a plausible but wrong heading. (QMC5883L is LSB first per the legacy v1 code; a different chip needs a different decoder.)
- -4096 on any axis means overflow. Drop the sample and do not feed it into calibration min/max. If it persists, a magnet or battery is too close, or choose a higher GN (lower gain).
- Reading only part of the 6 data bytes, or reading the MODE register, sets LOCK. New data then stops (DRDY/RDY affected) until all 6 bytes are read or MODE/CRA is rewritten. Always do one 6-byte burst from 0x03. Never read back MODE in the loop. Add a 'no sample for >3/ODR' recovery (full read + rewrite MODE).
- After a CRB (gain) write, the next measurement still uses the old gain. Discard it. After any CRA/CRB/MODE write in continuous mode, the first data comes after 2/f_DO (133 ms at 15 Hz).
- DRDY on HMC5883L is active LOW, a 250 µs pulse: use attachInterrupt(..., FALLING) with an IRAM_ATTR ISR. The v1 QMC driver used RISING. With autodetection, the interrupt edge must depend on the detected chip, or poll STATUS.RDY (0x09 bit0) instead.
- arduino-esp32 3.x: endTransmission(false) always returns 0 and holds the bus lock until requestFrom(). Check the requestFrom() return count for errors, and always pair the two calls in the same task. Probe presence with a plain beginTransmission/endTransmission() (with stop), which does report NACK (2).
- MR7 ('HS') and MR6..MR2, CRA bit7 and CRB bits4..0 must be written as 0. The datasheet both offers HS 3.4 MHz and says Hs is not supported.
- Do not leave the chip in continuous mode on other screens or before deep sleep (v1 grabli #4). Write MODE=0x02 (idle, 2 µA typ). Re-enabling needs MODE=0x00, plus a 2/f_DO wait.
- atan2(Y, X) assumes the chip frame Z-up and the board flat face-up with X forward. If the GY-273 is mounted face-down or rotated in the enclosure, the heading is mirrored or offset. Keep axis swap/sign in one config place and verify physically: X should be maximal pointing to magnetic north, and heading should increase when turning clockwise.
- No accelerometer, so no tilt compensation. In Moscow (inclination 71.7°) each 1° of tilt costs up to about 3° of heading, and 5° costs about 15°. Show a 'hold level' hint. Do not present the heading as more precise than about ±5°.
- Hard-iron can exceed the Earth's horizontal field (only about 181 counts at gain 1 in Moscow). Without calibration the needle 'jams' in a limited arc. Calibrate in the final assembly (battery, enclosure, screws, OLED), store the result, and offer recalibration.
- Min/max calibration needs a full flat 360° turn. Reject the result if either span is less than about 60% of the expected 2·H·gain (≈360 counts at gain 1 in Moscow) or the spans differ wildly. One glitch sample ruins min/max, so filter out implausible samples. It does not correct rotated (cross-axis) soft-iron: 8% coupling leaves about 5° residual.
- Declination is east-positive: true = magnetic + declination (Moscow about +12.1° in 2026). Do not subtract it. Make it a user setting.
- Never EMA heading degrees directly (the 359→0 wrap makes the needle swing through 180°). Filter cos/sin and take atan2, or use a wrap-aware delta. Use α=1-exp(-dt/τ) for an irregular dt.
- Do not copy LSB/G constants from Adafruit_HMC5883_U (1100/980). The HMC5883L datasheet value is 1090 LSB/G for all axes at GN=1. This matters for \|B\| sanity checks.
- The GY-273 may carry a QMC5883L/QMC5883P instead of an HMC5883L, so detect at runtime: 0x1E + ID 'H43' means HMC. Anything else must be shown on screen (which chip/addresses answered), not only logged.

**Unverified (researcher)**

- Whether the GY-273 delivered is really an HMC5883L. The listing says QMC5883L and the silkscreen says HMC5883L. It can only be settled on hardware with an I2C scan plus the ID read ('H43' at 0x0A).
- GY-273 board-level details were not checked against any schematic: on-board pull-up values, LDO, whether DRDY reaches the header at a 3.3 V-safe level, and whether the silkscreen X/Y arrows match the chip axes.
- My reading of the datasheet p.4 drawing (Z out of the top face, right-handed, so heading = atan2(Y,X) face-up) comes from the figure only (confidence medium). Hardware check: flat face-up in Moscow, Z should be negative (about -550 counts at gain 1), X largest pointing at magnetic north, heading rising when turned clockwise.
- Current draw at the recommended CRA=0x70 (8-sample averaging, 15 Hz) is not specified. The datasheet gives 100 µA only for 7.5 Hz with no averaging.
- Whether 8-sample averaging (MA=11) is compatible with the 75 Hz ODR setting is not stated in the datasheet.
- The MODE register value after a single measurement completes (0x02 or 0x03) is not stated ('changed to idle mode by setting MD[n] bits'). Reading MODE also sets LOCK, so it should not be used for polling anyway.
- Noise figures for averaged samples (~1.1-1.5 mG) were read off the datasheet graph on p.8 and are approximate.
- QMC5883L DRDY polarity and data format are outside this domain. The 'RISING' and 'LSB first' notes come only from legacy v1 code, not from a QMC datasheet.
- The test sketch was compiled, never run on hardware. The self-test discard timing (3 reads × 70 ms) and the DRDY-loss watchdog behaviour are untested on a real chip.
- Where the Adafruit 1100/980 LSB/G constants come from (they look like LSM303DLHC values) is my inference. That they do not match the HMC5883L datasheet is verified.
- That the HMC5883L is discontinued is supported only by secondary retail/tutorial sources, not by a Honeywell EOL notice.
- The Moscow, St Petersburg and Novosibirsk declination/inclination values come from my own WMM2025 implementation. It matched every NOAA test value, but I did not cross-check those cities with the NOAA online calculator. The yearly drift of declination was not quantified.

**Recommended snippets**

_HMC5883L raw-I2C driver + compass math for ESP32-S3 (Wire SDA=8/SCL=7, DRDY=GPIO12): probe/ID, init 0x70/0x20/0x00, X-Z-Y read with overflow check, single-shot, self-test, idle, LOCK watchdog, min/max cal, heading+declination, sin/cos LPF (file /mnt/wd/arduino-esp32/tmp/hmc5883l/t1/t1.ino). The watchdog in this test sketch is naive: it retries every loop until a sample arrives, so rate-limit it in production._ (compiled: yes)

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

namespace hmc {
constexpr uint8_t ADDR       = 0x1E;  // 7-bit (0x3C write / 0x3D read in 8-bit form)
constexpr uint8_t REG_CRA    = 0x00;
constexpr uint8_t REG_CRB    = 0x01;
constexpr uint8_t REG_MODE   = 0x02;
constexpr uint8_t REG_DATA   = 0x03;  // X MSB, X LSB, Z MSB, Z LSB, Y MSB, Y LSB  (order X,Z,Y!)
constexpr uint8_t REG_STATUS = 0x09;  // bit1 LOCK, bit0 RDY
constexpr uint8_t REG_ID_A   = 0x0A;  // 'H','4','3' at 0x0A..0x0C
// CRA: [7]=0 | MA1:MA0 (avg 1/2/4/8) | DO2:DO0 (rate) | MS1:MS0 (bias)
constexpr uint8_t CRA_AVG8   = 0x3 << 5;
constexpr uint8_t CRA_DO_15HZ = 0x4 << 2;   // 0.75,1.5,3,7.5,15,30,75 Hz for 0..6
constexpr uint8_t CRA_DO_30HZ = 0x5 << 2;
constexpr uint8_t CRA_MS_NORMAL = 0x0, CRA_MS_POS_BIAS = 0x1, CRA_MS_NEG_BIAS = 0x2;
// CRB: GN2:GN0 in bits 7..5, bits 4..0 must be 0
constexpr uint8_t CRB_GAIN(uint8_t gn) { return (uint8_t)(gn << 5); }
constexpr float LSB_PER_GAUSS[8] = {1370, 1090, 820, 660, 440, 390, 330, 230};
// MODE: bits 7..2 = 0 (MR7 'HS' must stay 0), MD1:MD0
constexpr uint8_t MODE_CONTINUOUS = 0x00, MODE_SINGLE = 0x01, MODE_IDLE = 0x02;
constexpr int16_t OVERFLOW_VALUE = -4096;    // ADC over/underflow marker per axis
}  // namespace hmc

static TwoWire &bus = Wire;           // SDA=8, SCL=7 (shared with SHT45)
static constexpr int PIN_SDA = 8, PIN_SCL = 7, PIN_DRDY = 12;
static constexpr uint8_t GAIN_INDEX = 1;     // +-1.3 Ga, 1090 LSB/G (power-on default)

static volatile bool g_drdy = false;
static void IRAM_ATTR onDrdy() { g_drdy = true; }   // DRDY pulses LOW ~250 us when data ready

static bool hmcWrite(uint8_t reg, uint8_t val) {
  bus.beginTransmission(hmc::ADDR);
  bus.write(reg);
  bus.write(val);
  return bus.endTransmission() == 0;   // 0 = ACK, 2 = address NACK, 5 = timeout
}

// Repeated-start read. NOTE: on arduino-esp32 3.x endTransmission(false) only queues the
// write and always returns 0; real errors show up as a short requestFrom() count.
static bool hmcRead(uint8_t reg, uint8_t *buf, size_t n) {
  bus.beginTransmission(hmc::ADDR);
  bus.write(reg);
  bus.endTransmission(false);
  if (bus.requestFrom(hmc::ADDR, n) != n) return false;
  for (size_t i = 0; i < n; ++i) buf[i] = (uint8_t)bus.read();
  return true;
}

enum class HmcStatus { Ok, NoAck, WrongId, BusError };

static HmcStatus hmcProbe() {
  bus.beginTransmission(hmc::ADDR);
  if (bus.endTransmission() != 0) return HmcStatus::NoAck;
  uint8_t id[3];
  if (!hmcRead(hmc::REG_ID_A, id, 3)) return HmcStatus::BusError;
  if (id[0] != 'H' || id[1] != '4' || id[2] != '3') return HmcStatus::WrongId;   // 0x48 0x34 0x33
  return HmcStatus::Ok;
}

static bool hmcBegin() {
  if (hmcProbe() != HmcStatus::Ok) return false;
  bool ok = hmcWrite(hmc::REG_CRA, hmc::CRA_AVG8 | hmc::CRA_DO_15HZ | hmc::CRA_MS_NORMAL)   // 0x70
         && hmcWrite(hmc::REG_CRB, hmc::CRB_GAIN(GAIN_INDEX))                               // 0x20
         && hmcWrite(hmc::REG_MODE, hmc::MODE_CONTINUOUS);                                  // 0x00
  return ok;   // first set ready after ~2/f_DO (133 ms at 15 Hz)
}

static bool hmcSleep() { return hmcWrite(hmc::REG_MODE, hmc::MODE_IDLE); }   // ~2 uA

struct MagRaw { int16_t x, y, z; bool overflow; };

static bool hmcReadRaw(MagRaw &m) {
  uint8_t b[6];
  if (!hmcRead(hmc::REG_DATA, b, 6)) return false;   // always all 6 bytes (else LOCK)
  m.x = (int16_t)((b[0] << 8) | b[1]);
  m.z = (int16_t)((b[2] << 8) | b[3]);
  m.y = (int16_t)((b[4] << 8) | b[5]);
  m.overflow = (m.x == hmc::OVERFLOW_VALUE || m.y == hmc::OVERFLOW_VALUE || m.z == hmc::OVERFLOW_VALUE);
  return true;
}

// ---- compass math ----
struct MagCal {
  float offX = 0, offY = 0, sclX = 1, sclY = 1;   // hard-iron offsets, soft-iron per-axis scale
  int16_t minX = INT16_MAX, maxX = INT16_MIN, minY = INT16_MAX, maxY = INT16_MIN;
  void reset() { *this = MagCal(); }
  void feed(int16_t x, int16_t y) {
    if (x < minX) minX = x; if (x > maxX) maxX = x;
    if (y < minY) minY = y; if (y > maxY) maxY = y;
  }
  bool finish(int16_t minSpan) {
    const float sx = (float)maxX - minX, sy = (float)maxY - minY;
    if (sx < minSpan || sy < minSpan) return false;     // not rotated a full turn
    offX = 0.5f * ((float)maxX + minX);
    offY = 0.5f * ((float)maxY + minY);
    const float avg = 0.5f * (sx + sy);
    sclX = avg / sx;
    sclY = avg / sy;
    return true;
  }
};

static inline float wrap360(float d) { d = fmodf(d, 360.0f); return d < 0 ? d + 360.0f : d; }

// Chip frame (Honeywell datasheet p.4): X and Y in the package plane, Z out of the top face.
// Board flat, top face up, X pointing forward => Y points LEFT, heading = atan2(Y, X), clockwise from N.
static float headingDeg(const MagRaw &m, const MagCal &c, float declinationDeg) {
  const float x = (m.x - c.offX) * c.sclX;
  const float y = (m.y - c.offY) * c.sclY;
  return wrap360(atan2f(y, x) * (180.0f / (float)M_PI) + declinationDeg);
}

// Angle low-pass on the unit vector: no 359->0 wrap jump.
struct AngleLpf {
  float c = 1, s = 0; bool init = false;
  float update(float deg, float dtSec, float tauSec) {
    const float r = deg * (float)M_PI / 180.0f;
    if (!init) { c = cosf(r); s = sinf(r); init = true; }
    const float a = 1.0f - expf(-dtSec / tauSec);
    c += a * (cosf(r) - c);
    s += a * (sinf(r) - s);
    return wrap360(atan2f(s, c) * 180.0f / (float)M_PI);
  }
};

static MagCal g_cal;
static AngleLpf g_lpf;

// Single-measurement: write MODE=0x01, data ready after ~6 ms (DRDY falls), chip returns to idle by itself.
static bool hmcSingleShot(MagRaw &m, uint32_t timeoutMs = 20) {
  g_drdy = false;
  if (!hmcWrite(hmc::REG_MODE, hmc::MODE_SINGLE)) return false;
  const uint32_t t0 = millis();
  while (!g_drdy) {
    if (millis() - t0 > timeoutMs) {             // DRDY not wired/working: fall back to status RDY bit
      uint8_t st;
      if (hmcRead(hmc::REG_STATUS, &st, 1) && (st & 0x01)) break;
      if (millis() - t0 > 2 * timeoutMs) return false;
    }
    delay(1);
  }
  return hmcReadRaw(m);
}

// Datasheet self-test: CRA=0x71 (8 avg, 15 Hz, positive bias), CRB=0xA0 (gain 5, 390 LSB/G),
// expect 243..575 LSB on X, Y, Z. Restore CRA=0x70 and the working gain afterwards.
static bool hmcSelfTest(MagRaw &out) {
  bool ok = hmcWrite(hmc::REG_CRA, 0x71) && hmcWrite(hmc::REG_CRB, hmc::CRB_GAIN(5)) &&
            hmcWrite(hmc::REG_MODE, hmc::MODE_CONTINUOUS);
  MagRaw m{};
  for (int i = 0; ok && i < 3; ++i) {            // first set(s) may still use the old gain: discard
    delay(70);                                   // 15 Hz -> ~67 ms per set
    ok = hmcReadRaw(m);
  }
  out = m;
  const bool pass = ok && !m.overflow && m.x >= 243 && m.x <= 575 && m.y >= 243 && m.y <= 575 &&
                    m.z >= 243 && m.z <= 575;
  hmcWrite(hmc::REG_CRA, hmc::CRA_AVG8 | hmc::CRA_DO_15HZ | hmc::CRA_MS_NORMAL);
  hmcWrite(hmc::REG_CRB, hmc::CRB_GAIN(GAIN_INDEX));
  return pass;                                   // next sample after this uses old gain: discard it
}

// If DRDY stops (e.g. a partial read left the data registers LOCKed), a full 6-byte read
// or re-writing MODE unlocks them (datasheet status register, LOCK bit).
static void hmcWatchdog(uint32_t lastSampleMs) {
  if (millis() - lastSampleMs > 500) {
    MagRaw dummy;
    hmcReadRaw(dummy);
    hmcWrite(hmc::REG_MODE, hmc::MODE_CONTINUOUS);
  }
}

void setup() {
  Serial.begin(115200);
  bus.begin(PIN_SDA, PIN_SCL, 400000);
  pinMode(PIN_DRDY, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_DRDY), onDrdy, FALLING);
  HmcStatus st = hmcProbe();
  Serial.printf("HMC5883L probe: %d\n", (int)st);
  MagRaw st_raw;
  Serial.printf("self-test %s x=%d y=%d z=%d\n", hmcSelfTest(st_raw) ? "PASS" : "FAIL", st_raw.x, st_raw.y, st_raw.z);
  MagRaw one;
  if (hmcSingleShot(one)) Serial.printf("single x=%d y=%d z=%d\n", one.x, one.y, one.z);
  if (!hmcBegin()) Serial.println("HMC5883L init failed");
}

void loop() {
  static uint32_t lastMs = millis();
  hmcWatchdog(lastMs);
  if (g_drdy) {
    g_drdy = false;
    MagRaw m;
    if (hmcReadRaw(m) && !m.overflow) {
      uint32_t now = millis();
      float h = headingDeg(m, g_cal, 12.1f);
      float hs = g_lpf.update(h, (now - lastMs) / 1000.0f, 0.3f);
      lastMs = now;
      Serial.printf("x=%d y=%d z=%d  hdg=%.1f  smooth=%.1f  |B|=%.3f G\n", m.x, m.y, m.z, h, hs,
                    sqrtf((float)m.x * m.x + (float)m.y * m.y + (float)m.z * m.z) / hmc::LSB_PER_GAUSS[GAIN_INDEX]);
    }
  }
}
```

_Host (g++) verification of the compass math, not ESP32 code: Moscow WMM field model, check of the atan2(Y,X) convention for the chip frame (X fwd, Y left, Z up), tilt-error table, min/max calibration with and without cross-axis soft-iron, wrap-safe LPF (scratchpad/scripts/compass_math_test.cpp; compiled with g++ -O2 -std=c++17 and run)_ (compiled: yes)

```cpp
// Key parts (full file: scratchpad/scripts/compass_math_test.cpp)
// b = Rx(phi) Ry(theta) Rz(psi) B   (AN4248 NED body frame: x fwd, y right, z down)
// V toHmc(V b) { return { b.x, -b.y, -b.z }; }   // HMC chip: X fwd, Y left, Z up
// double hdg(V h){ double d=atan2(h.y,h.x)*R2D; return d<0?d+360:d; }
// Output:
// flat: max |atan2(Y,X) - true heading| = 0.000000 deg
// flat facing N: X=16642 Y=-0 Z=-50439 nT (Z<0 in N hemisphere with Z-up)
// inclination 71.74 deg, tan=3.031
// tilt  1.0 deg: max heading err pitch=3.03 roll=3.03
// tilt  5.0 deg: max heading err pitch=15.38 roll=15.38
// tilt 10.0 deg: max heading err pitch=32.31 roll=32.31
// cal cross=0.00: ... max err raw=179.4 deg, after min/max cal=0.00 deg
// cal cross=0.08: ... max err raw=179.6 deg, after min/max cal=5.21 deg
// LPF step 350->10: vector 356.0, 0.2, 3.2, 5.2 ...  naive 248.0, 176.6, 126.6 ...
// noise 1.1 mG rms / H 166.4 mG -> 0.38 deg rms
```


## esp32s3-power

| id | status | claim | method/conf | source |
|---|---|---|---|---|
| F01 | SINGLE-SOURCE | Arduino-ESP32 core 3.3.12 bundles ESP-IDF v5.5.5, commit b774170ff46. The GitHub tag v5.5.5 resolves to the same SHA. A static_assert(ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5,5,5)) compiled. | compile-test/high | /mnt/wd/arduino-esp32/data/packages/esp32/tools/esp32s3-libs/3.3.12/versions.txt; .../include/esp_common/include/esp_idf_version.h:14-18; api.github.com/repos/espressif/esp-idf/commits/v5.5.5 |
| F02 | SINGLE-SOURCE | The prebuilt S3 libs set: CONFIG_PM_ENABLE off (no auto light sleep, no esp_pm), CONFIG_PM_SLP_IRAM_OPT off, CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP=y, CONFIG_RTC_CLK_SRC_INT_RC=y, CONFIG_FREERTOS_HZ=1000, CONFIG_ESP_TIME_FUNCS_USE_RTC_TIMER=y and _USE_ESP_TIMER=y. CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND is off. CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ=240. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/tools/esp32s3-libs/3.3.12/sdkconfig lines 1963, 1973, 2090-2092, 2157, 2263-2264, 2433 |
| F03 | SINGLE-SOURCE | For the esp32s3 FQBN, USBMode defaults to hwcdc (build.usb_mode=1). With CDCOnBoot=cdc the build gets -DARDUINO_USB_MODE=1 -DARDUINO_USB_CDC_ON_BOOT=1, so Serial is HWCDCSerial, the built-in USB-Serial/JTAG controller, not TinyUSB. | compile-test/high | .../hardware/esp32/3.3.12/boards.txt:1203,1283-1291; cores/esp32/HardwareSerial.h:441-444; build compile_commands.json contains ARDUINO_USB_MODE=1 |
| F04 | SINGLE-SOURCE | On the ESP32-S3, RTC IOs are GPIO0..21 (RTC_GPIOn = GPIOn). Only these pins work for ext0/ext1. All 4 buttons (GPIO1,4,5,6) and DRDY (GPIO12) are RTC-capable. esp_sleep_is_valid_wakeup_gpio(g) returns RTC_GPIO_IS_VALID_GPIO(g). | datasheet/high | ESP32-S3 Datasheet v2.2 Table 2-6 RTC Functions (https://documentation.espressif.com/esp32-s3_datasheet_en.pdf); esp_sleep.h:316; sleep_modes.c v5.5.5 esp_sleep_is_valid_wakeup_gpio |
| F05 | SINGLE-SOURCE | ext1 modes on S3: ESP_EXT1_WAKEUP_ANY_LOW=0 and ESP_EXT1_WAKEUP_ANY_HIGH=1. ESP_EXT1_WAKEUP_ALL_LOW is a deprecated alias of ANY_LOW, and using it gave a -Wdeprecated-declarations warning in a compile test. The S3 has no per-pin levels (no SOC_PM_SUPPORT_EXT1_WAKEUP_MODE_PER_PIN), so esp_sleep_enable_ext1_wakeup_io returns ESP_ERR_NOT_ALLOWED if the pins would get different levels. | compile-test/high | include/esp_hw_support/include/esp_sleep.h:36-41,379-380; compile of t2 sketch |
| F06 | SINGLE-SOURCE | esp_sleep_enable_ext1_wakeup(mask, mode) first clears all ext1 config (esp_sleep_disable_ext1_wakeup_io(0)) and then calls esp_sleep_enable_ext1_wakeup_io. The header says the old function 'will be deprecated in release/v6.0'. Use esp_sleep_enable_ext1_wakeup_io (appends pins) and esp_sleep_disable_ext1_wakeup_io (0 clears all). A non-RTC GPIO in the mask returns ESP_ERR_INVALID_ARG and logs 'Not an RTC IO: GPIO%d'. | source-or-header/high | sleep_modes.c v5.5.5 (github.com/espressif/esp-idf/blob/v5.5.5/components/esp_hw_support/sleep_modes.c) esp_sleep_enable_ext1_wakeup / _io; esp_sleep.h:309 |
| F07 | SINGLE-SOURCE | These do NOT exist on ESP32-S3 in IDF 5.5.5 and failed a compile test: esp_deep_sleep_enable_gpio_wakeup, ESP_GPIO_WAKEUP_GPIO_LOW, esp_sleep_get_gpio_wakeup_status and esp_sleep_enable_ext1_wakeup_with_level_mask. On S3, buttons can wake deep sleep only through ext1 (several pins, one level) or ext0 (one pin). | compile-test/high | negative compile test /mnt/wd/arduino-esp32/tmp/esp32s3-power/t3/t3.ino; esp_sleep.h:447 (#if SOC_GPIO_SUPPORT_DEEPSLEEP_WAKEUP), :403 |
| F08 | SINGLE-SOURCE | ext1_wakeup_prepare(), run inside esp_deep_sleep_start/esp_light_sleep_start, moves each ext1 pad to the RTC mux (rtcio_hal_function_select RTC) and enables its input. If RTC_PERIPH pd_option != ON it also enables RTC pad hold on that pin. It does NOT configure pull-ups or pull-downs; the application must do that beforehand with rtc_gpio_pullup_en/pulldown_dis. | source-or-header/high | sleep_modes.c v5.5.5 ext1_wakeup_prepare() (~lines 2078-2125) |
| F09 | SINGLE-SOURCE | On the S3, digital (IO_MUX) pulls and RTC-IO pulls are separate registers (SOC_GPIO_SUPPORT_RTC_INDEPENDENT=1). gpio_pullup_en() writes only the digital pull, and so do Arduino pinMode(INPUT_PULLUP) and gpio_config(). The RTC-IO pull that applies after ext1 moves the pad to the RTC mux must be set with rtc_gpio_pullup_en(). | source-or-header/high | soc/esp32s3/include/soc/soc_caps.h:181-182 (v5.5.5); esp_driver_gpio/src/gpio.c v5.5.5 gpio_pullup_en lines 83-97 |
| F10 | SINGLE-SOURCE | The IDF v5.5.5 docs say internal pulls on ext1 pins work in both cases. With RTC_PERIPH ON, the RTC-IO pulls stay powered. With RTC_PERIPH OFF (the default for ext1-only), IDF holds the pad so the pull setting is latched, which uses less power. The official example uses rtc_gpio_init + rtc_gpio_pulldown_dis + rtc_gpio_pullup_en and does NOT force RTC_PERIPH ON. So esp_sleep_pd_config(RTC_PERIPH, ON) is optional in IDF 5.5.5, not required as the v1 note in BRIEF section 4.5 says. This has not been checked on hardware. | official-docs/high | docs/en/api-reference/system/sleep_modes.rst v5.5.5 lines 238-247; examples/system/deep_sleep/main/ext_wakeup.c v5.5.5 |
| F11 | SINGLE-SOURCE | With RTC_PERIPH left at AUTO, IDF keeps it ON only when the EXT0 or GPIO (light-sleep) wake source is enabled. With ext1 and timer only, it is powered down. | source-or-header/high | sleep_modes.c v5.5.5 ~lines 2694-2700 |
| F12 | SINGLE-SOURCE | Chip-only typical currents from ESP32-S3 datasheet v2.2 Table 5-10: Light-sleep 240 uA; Deep-sleep with RTC memory and RTC peripherals powered 8 uA; Deep-sleep with RTC peripherals off 7 uA; power off (CHIP_PU low) 1 uA. Forcing RTC_PERIPH ON therefore costs about 1 uA typical. | datasheet/high | ESP32-S3 Series Datasheet v2.2, Table 5-10 Current Consumption in Low-Power Modes, p.67-68 |
| F13 | SINGLE-SOURCE | The internal weak pull-up and pull-down are 45 kOhm typical. A button held LOW during deep sleep with the internal pull-up draws about 3.3 V / 45 kOhm = 73 uA. | datasheet/high | ESP32-S3 Datasheet v2.2, DC characteristics (R_PU/R_PD) |
| F14 | SINGLE-SOURCE | After a deep-sleep wake, IDF startup calls esp_deep_sleep_wakeup_io_reset(), which clears the ext1 wake-pin selection and disables the hold on those pins. Arduino pinMode() calls gpio_config(), which calls rtc_gpio_deinit() for RTC pins and routes the pad back to digital. So pinMode(btn, INPUT_PULLUP) in setup() is enough after a deep-sleep wake. | source-or-header/high | components/esp_system/port/cpu_start.c v5.5.5 lines 822-826; components/esp_hw_support/sleep_gpio.c v5.5.5 esp_deep_sleep_wakeup_io_reset; esp_driver_gpio/src/gpio.c v5.5.5 lines 372-374 |
| F15 | SINGLE-SOURCE | If ext1 is used for LIGHT sleep with RTC_PERIPH off, the wake pins stay held and in the RTC mux after waking. Call rtc_gpio_hold_dis(), then rtc_gpio_deinit() or pinMode(), before using them as digital inputs again. | official-docs/high | sleep_modes.rst v5.5.5 line 272 |
| F16 | SINGLE-SOURCE | esp_sleep_get_ext1_wakeup_status() returns a GPIO-numbered bitmask (BIT(n) for GPIOn) when the cause is ESP_SLEEP_WAKEUP_EXT1, otherwise 0; use __builtin_ctzll(mask) to get the button's GPIO. esp_sleep_get_wakeup_cause() returns only one cause. IDF 5.5.5 adds uint32_t esp_sleep_get_wakeup_causes(), a bitmap that is linked in the test ELF. After a cold boot the cause is ESP_SLEEP_WAKEUP_UNDEFINED. | compile-test/high | esp_sleep.h:583-590,713-728; sleep_modes.c v5.5.5 esp_sleep_get_ext1_wakeup_status; nm of t1.ino.elf |
| F17 | SINGLE-SOURCE | Light-sleep GPIO wake works on any GPIO, RTC or digital. Call gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL or GPIO_INTR_HIGH_LEVEL) per pin, then esp_sleep_enable_gpio_wakeup(). Edge types return ESP_ERR_INVALID_ARG. The cause is ESP_SLEEP_WAKEUP_GPIO. The S3 has no API that reports which GPIO woke it from light sleep, so read the pin levels after waking (the level is still held by the pressed button). On RTC pins, gpio_wakeup_enable also calls rtc_gpio_wakeup_enable. | source-or-header/high | esp_driver_gpio/src/gpio.c v5.5.5 gpio_wakeup_enable lines 650-679; esp_sleep.h:479-502; sleep_modes.rst v5.5.5 line 294 |
| F18 | SINGLE-SOURCE | gpio_wakeup_enable() overwrites the pin's GPIO interrupt type with the LOW/HIGH level type (gpio_hal_set_intr_type). gpio_wakeup_disable() does NOT restore it. | source-or-header/high | esp_driver_gpio/src/gpio.c v5.5.5 lines 666-668 and 681-700 |
| F19 | SINGLE-SOURCE | Wake sources stay armed after a wakeup and carry over to later light or deep sleeps until esp_sleep_disable_wakeup_source() is called; ESP_SLEEP_WAKEUP_ALL clears them all. esp_sleep_pd_config() settings also persist until the next reboot (static s_config). | official-docs/high | sleep_modes.rst v5.5.5 line 170 |
| F20 | SINGLE-SOURCE | Timer wakeup: esp_sleep_enable_timer_wakeup(us), measured on the RTC timer (48-bit). For S3 light sleep the requested time is shortened by a calculated overhead: DEFAULT_HARDWARE_OUT_OVERHEAD_US=133, plus DEFAULT_SLEEP_OUT_OVERHEAD_US=8628 when CONFIG_PM_SLP_IRAM_OPT is off (the Arduino case), plus the measured entry time. The out-overhead is measured again after each successful wake. If the adjusted time is at or below the minimum, esp_light_sleep_start() returns ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION without sleeping. Very short timer sleeps (under about 10 ms on the first call) are therefore rejected. | source-or-header/high | sleep_modes.c v5.5.5 lines 182-188, 1496-1531, 1592-1609 |
| F21 | SINGLE-SOURCE | esp_timer_get_time() is compensated for manual light sleep. On wake, IDF sets the esp_timer counter to its pre-sleep value plus the slept time measured on the RTC slow clock (esp_timer_private_set), then refreshes gettimeofday() (esp_set_time_from_rtc). millis() and micros() are built on esp_timer, so they are compensated too. esp_timer callbacks do not run during light sleep and do not wake the chip; overdue callbacks run after wake. | official-docs/high | sleep_modes.c v5.5.5 lines 1615-1628; docs/en/api-reference/system/esp_timer.rst v5.5.5 lines 150-154 |
| F22 | SINGLE-SOURCE | In core 3.3.12, millis() is (unsigned long)(esp_timer_get_time()/1000) and wraps after 2^32 ms, about 49.7 days. micros() is the 32-bit truncation of esp_timer_get_time() and wraps after about 71.6 minutes. delay(ms) is vTaskDelay. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/cores/esp32/esp32-hal-misc.c lines 204-214 |
| F23 | SINGLE-SOURCE | The FreeRTOS tick count is NOT advanced by manual esp_light_sleep_start(). The OS tick runs on systimer counter 1 (SYSTIMER_COUNTER_OS_TICK), and sleep_modes.c corrects only counter 0 (SYSTIMER_COUNTER_ESPTIMER); sleep_modes.c never calls vTaskStepTick. xTaskGetTickCount() and FreeRTOS software timers therefore exclude slept time. Use esp_timer_get_time() for intervals. | source-or-header/medium | esp_hw_support/include/esp_private/systimer.h:30-31 (installed); esp_timer_impl_systimer.c v5.5.5 esp_timer_impl_set; grep of sleep_modes.c v5.5.5 shows no StepTick |
| F24 | SINGLE-SOURCE | The RTC slow clock in this build is the internal RC, about 136 kHz on S3 (CONFIG_RTC_CLK_SRC_INT_RC). With PM disabled it is calibrated against XTAL before every sleep using RTC_CLK_SRC_CAL_CYCLES=10 slow cycles. IDF says this source drifts with temperature in both light and deep sleep, so time measured across sleep (stopwatch, wall clock) carries RC error. | official-docs/high | ESP32-S3 Datasheet v2.2 sec. RTC clock ('Internal slow RC oscillator (typically about 136 kHz, adjustable)'); sleep_modes.c v5.5.5 lines 170, 848-855; system_time.rst v5.5.5 line 34 |
| F25 | SINGLE-SOURCE | Deep sleep ends in a reboot. esp_timer_get_time() and millis() restart from about 0 because they count from esp_timer init. gettimeofday() and time() keep running, because the boot-time offset sits in RTC_BOOT_TIME_LOW/HIGH registers and the RTC timer survives every reset except power-on. RTC_DATA_ATTR variables survive deep sleep; RTC slow memory stays powered by default when used. esp_rtc_get_time_us() (esp_rtc_time.h) is a monotonic microsecond counter that keeps counting across deep sleep. | official-docs/high | newlib/src/port/esp_time_impl.c v5.5.5 esp_time_impl_set_boot_time; system_time.rst v5.5.5 line 11; esp_timer.h:220-223; sleep_modes.rst v5.5.5 power-down section; esp_hw_support/include/esp_rtc_time. |
| F26 | SINGLE-SOURCE | USB-Serial/JTAG in light sleep on S3: the S3 lacks SOC_USB_SERIAL_JTAG_SUPPORT_LIGHT_SLEEP. Before light sleep IDF therefore calls sleep_console_usj_pad_backup_and_disable() (USB_SERIAL_JTAG.conf0.usb_pad_enable=0, USJ bus clock off) and restores both after wake; this function is linked into the test ELF. IDF does not refuse sleep while USB is connected. The docs say the device is unresponsive during light sleep, the host may report it disconnected or faulty, and after wake the host may not re-enumerate, in which case the cable must be re-plugged. | official-docs/high | sleep_modes.c v5.5.5 lines 734-736, 796-797; sleep_console.c v5.5.5; docs/en/api-guides/usb-serial-jtag-console.rst v5.5.5 lines 112-122; nm t1.ino.elf |
| F27 | SINGLE-SOURCE | Deep sleep powers off the USJ controller and PHY, and the D+ pull-up drops. The host sees the device disconnect and reconnect on wake (reboot). | official-docs/high | usb-serial-jtag-console.rst v5.5.5 lines 101-107 |
| F28 | SINGLE-SOURCE | usb_serial_jtag_is_connected() (= HWCDC::isPlugged()) returns true while USB SOF packets arrive. That means any USB host or hub, even with no serial terminal open. It returns false on a power bank or charger. A FreeRTOS tick hook updates it and reports 'disconnected' only after USJ_DISCONNECT_CONFIRM_PERIOD_MS=3 ticks without SOF. It starts as true at boot. | source-or-header/high | esp_driver_usb_serial_jtag/include/driver/usb_serial_jtag.h:94-106 (installed); usb_serial_jtag_connection_monitor.c v5.5.5 lines 25, 86; cores/esp32/HWCDC.h:73-77 |
| F29 | SINGLE-SOURCE | HWCDC operator bool() returns isCDC_Connected(). That is false if !isPlugged(); otherwise it returns an internal 'connected' flag. The flag starts false. The ISR sets it true when the host picks up an IN packet (SERIAL_IN_EMPTY) or sends data (OUT_RECV_PKT). It goes false on BUS_RESET, on a flush() stall (tx_timeout_ms=100 x 1 ms tries with no progress), or on write timeouts while unplugged. When not connected, write() and print() do not block: bytes go into the TX ring buffer with a drop-oldest policy. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/cores/esp32/HWCDC.cpp lines 48,105,148,239,248,268-290,344-353,374-377,538-556,625-660 |
| F30 | SINGLE-SOURCE | setCpuFrequencyMhz() on S3 in core 3.3.12 does no USB-related check. 240, 160 and 80 come from the 480 MHz PLL and keep APB at 80 MHz. XTAL-derived values (40, 20, 10 with a 40 MHz XTAL; the board menu offers 240/160/80/40/20/10) set APB equal to the CPU frequency. The change goes through rtc_clk_cpu_freq_set_config_fast(), which for an XTAL source only switches the CPU clock and does not turn off the BBPLL that feeds the USJ 48 MHz PHY clock. getSupportedCpuFrequencyMhz() returns the runtime-valid list. | source-or-header/high | cores/esp32/esp32-hal-cpu.c 3.3.12 calculateApb() lines 198-225, setCpuFrequencyMhz() ~355-415; esp_hw_support/port/esp32s3/rtc_clk.c v5.5.5 lines 255-294, 365-376; boards.txt:1378-1389 |
| F31 | SINGLE-SOURCE | The Wire (I2C) driver in core 3.3.12 (esp32-hal-i2c-ng.c) uses I2C_CLK_SRC_DEFAULT, which is SOC_MOD_CLK_XTAL on S3. I2C bus timing therefore does not depend on CPU or APB frequency changes. | source-or-header/high | cores/esp32/esp32-hal-i2c-ng.c:126; include/soc/esp32s3/include/soc/clk_tree_defs.h:342 |
| F32 | SINGLE-SOURCE | Active current with dual cores idle (WAITI), typical: 240 MHz 32.9 mA, 160 MHz 27.6 mA, 80 MHz 22.0 mA, 40 MHz 13.2 mA with all peripheral clocks off. With all peripheral clocks on: 47.6, 42.3, 36.1 and 18.8 mA. Light sleep is 240 uA. | datasheet/high | ESP32-S3 Datasheet v2.2 Table 5-9 Current Consumption in Modem-sleep Mode, Table 5-10 |
| F33 | SINGLE-SOURCE | On S3, gpio_hold_en() on an RTC pad (GPIO0-21) calls rtc_gpio_hold_en(), and that hold persists through deep sleep. Digital-only pads (GPIO26-48) also need gpio_deep_sleep_hold_en(). A hold remains after a deep-sleep wake until gpio_hold_dis(). The exception is ext1 wake pins, which startup releases (F14). USB pads cannot be held LOW across deep sleep. | source-or-header/high | esp_driver_gpio/include/driver/gpio.h:386-411, 436-450 (installed); gpio.c v5.5.5 gpio_hold_en lines 744-762 |
| F34 | SINGLE-SOURCE | In light sleep, CPUs, RAM and digital peripherals are clock-gated and keep their state; esp_light_sleep_start() returns ESP_OK after wake. It stalls the other core and runs in a critical section, so all other tasks are frozen too. UART FIFOs are not flushed before light sleep, while deep sleep flushes them. Light sleep does not power down flash by default; deep sleep always does. | official-docs/high | sleep_modes.rst v5.5.5 'Sleep Modes', 'UART Output Handling', 'Power-down of Flash'; sleep_modes.c v5.5.5 line 1462 |
| F35 | SINGLE-SOURCE | In this build the automatic GPIO sleep-isolation switching is not compiled in (esp_sleep_startup_init with esp_sleep_enable_gpio_switch(true) requires CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND or CONFIG_PM_SLP_DISABLE_GPIO, and both are off). Digital pads should therefore keep their active configuration (IO_MUX pull-ups, output levels) through light sleep. | source-or-header/medium | sleep_gpio.c v5.5.5 '#if CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND \|\| CONFIG_PM_SLP_DISABLE_GPIO' block; sdkconfig line 1963 |
| F36 | SINGLE-SOURCE | The core's own example ExternalWakeUp.ino (3.3.12) contains an old comment saying internal pulls 'also require RTC peripherals to be turned on'. Further down, the same file says HOLD is used when RTC_PERIPH is off. The old comment is the likely source of the v1 RTC_PERIPH=ON note. | source-or-header/medium | .../hardware/esp32/3.3.12/libraries/ESP32/examples/DeepSleep/ExternalWakeUp/ExternalWakeUp.ino |
| F37 | SINGLE-SOURCE | The consolidated light-sleep + ext1 deep-sleep + USB-gate code below compiled and linked with --warnings all and no warnings for esp32:esp32:esp32s3:CDCOnBoot=cdc (309130 bytes flash). These symbols are confirmed in the ELF: esp_light_sleep_start, esp_sleep_enable_gpio_wakeup, gpio_wakeup_enable, esp_sleep_enable_ext1_wakeup_io, esp_sleep_get_wakeup_causes, rtc_gpio_pullup_en, gpio_deep_sleep_hold_en, usb_serial_jtag_is_connected, esp_rtc_get_time_us. | compile-test/high | /mnt/wd/arduino-esp32/tmp/esp32s3-power/t4_consolidated_snapshot.ino.txt; t1_lightsleep_snapshot.ino.txt; t2_deepsleep_snapshot.ino.txt; build /mnt/wd/arduino-esp32/build/hwfacts-esp32s3-power |

**Pitfalls**

- Do not rely on pinMode(INPUT_PULLUP) for deep sleep. On S3 it sets only the digital IO_MUX pull-up (F09), and ext1 moves the pad to the RTC mux (F08). Call rtc_gpio_pullup_en() and rtc_gpio_pulldown_dis() on every ext1 button right before sleep. The official example also calls rtc_gpio_init() first. Without that, the pins may float and cause random wakes or no wake. The external pull-ups on the OLED module are unknown.
- ext1 ANY_LOW with a button already held: esp_deep_sleep_start() sleeps and wakes at once, which looks like a reboot loop. Wait until all buttons read HIGH, then use esp_deep_sleep_try_to_start(), which returns ESP_ERR_SLEEP_REJECT when it cannot sleep.
- Light-sleep GPIO wake is level-triggered. If a button is LOW at entry, it wakes immediately. Check gpio_get_level() first, as the IDF light_sleep example does with example_wait_gpio_inactive().
- gpio_wakeup_enable() overwrites the pin's interrupt type with LOW_LEVEL, and gpio_wakeup_disable() does not restore it (F18). If the buttons use attachInterrupt(FALLING), restore the type after wake (gpio_set_intr_type(pin, GPIO_INTR_NEGEDGE) or attach again). Otherwise a held button can fire a level-interrupt storm. With polled buttons, set GPIO_INTR_DISABLE.
- Wake sources and esp_sleep_pd_config persist (F19). A timer wake armed for one light sleep stays armed for the next sleep, including deep sleep. Call esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL) before configuring each sleep.
- Do not mix ext1 into light sleep unless you handle the aftermath: with RTC_PERIPH off, the pins stay held and in the RTC mux after wake (F15). For light sleep, use gpio_wakeup_enable/esp_sleep_enable_gpio_wakeup. Keep ext1 for deep sleep, where IDF startup releases the pins.
- Use esp_timer_get_time() (int64 us) for intervals, timeouts and stopwatches. It and millis() are corrected after light sleep (F21), but xTaskGetTickCount(), FreeRTOS software timers and vTaskDelay in other tasks are not (F23). millis() wraps after 49.7 days and micros() after 71.6 min (F22).
- esp_timer callbacks neither wake the chip from manual light sleep nor run during it (F21). A countdown timer (R5) must arm esp_sleep_enable_timer_wakeup(remaining_us) itself if it has to fire on time while the device sleeps.
- Timer light sleeps shorter than about 10 ms may return ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION on S3 with the Arduino libs (DEFAULT_SLEEP_OUT_OVERHEAD_US=8628, F20). Check the return value; do not assume the device slept.
- Light-sleep time is measured with the internal ~136 kHz RC (F24). A stopwatch or timer that spans long light sleeps inherits RC drift. The same applies to wall-clock time across deep sleep.
- Deep sleep is a reboot. esp_timer and millis() restart at 0, and RAM state is lost except RTC_DATA_ATTR. A running stopwatch or timer must block deep sleep, or be persisted as wall-clock or esp_rtc_get_time_us() timestamps in RTC_DATA_ATTR (F25). Sensors must be put in standby before deep sleep (BRIEF v1 lesson 4).
- Light sleep while plugged into a PC makes USB-Serial/JTAG disappear or stall for the host. After wake the host may not re-enumerate, so the Serial monitor dies until the cable is re-plugged (F26). Gate sleep on !usb_serial_jtag_is_connected(): true only when a USB host sends SOF (F28), false on a charger or power bank.
- Never use while(!Serial) on this board. operator bool is false until the host reads an IN packet (F29), so on battery the loop never ends. Serial.print without a host does not block (drop-oldest ring buffer), but Serial.flush() with a host attached can wait up to about 100 ms of no progress.
- Call Serial.flush() before light or deep sleep if the last log lines matter. Light sleep does not flush anything for you (F34), and the USJ pad is disabled during sleep (F26).
- Do not drop below 80 MHz with setCpuFrequencyMhz() unless the change is hardware-tested. At 40/20/10 MHz, APB equals the CPU clock (F30). I2C runs from XTAL and is unaffected (F31), but USB-Serial/JTAG behaviour at those clocks is unverified. 80 MHz saves about 11 mA typical vs 240 MHz when idle (F32) and keeps APB at 80 MHz.
- ESP_EXT1_WAKEUP_ALL_LOW copied from ESP32 code compiles on S3 with only a deprecation warning and silently means ANY_LOW (F05). esp_deep_sleep_enable_gpio_wakeup does not exist on S3 (F07).
- gpio_hold_en() on RTC pads persists after a deep-sleep wake until gpio_hold_dis() (F33). If any non-wake pin is held before sleep, call gpio_hold_dis() on it in setup(), or it stays frozen.
- The current figures are chip-only (F12, F32). The SuperMini's LDO, power LED, OLED module and sensors add to them, so measure the real gadget before promising battery life.

**Unverified (researcher)**

- Hardware not tested: with RTC_PERIPH OFF (default), whether the IDF hold really keeps the RTC-IO pull-ups on GPIO1/4/5/6 active through S3 deep sleep. The IDF 5.5.5 docs say yes (F10). Cheapest check: deep-sleep with the pins pulled only internally, confirm there is no spurious wake and that each button wakes; repeat with keepRtcPeriphOn=true.
- Whether the host PC (user's Windows) re-enumerates the USB-Serial/JTAG device after a light sleep, or needs a re-plug. The docs say the D+ pull-up 'remains pulled up' in light sleep, but the v5.5.5 code sets usb_pad_enable=0 during light sleep. Whether that also drops the D+ pull-up was not checked.
- USB-Serial/JTAG (HWCDC) behaviour at CPU 40/20/10 MHz (XTAL source, APB = CPU). The BBPLL stays on via the fast path (F30), but nothing was run.
- Whether the host polls the CDC IN endpoint when no terminal is open. This decides whether `if (Serial)` turns true without an open monitor; it depends on the OS driver. Not checked.
- The exact list getSupportedCpuFrequencyMhz() prints on this chip. From source it is likely 240, 160, 80, 40, 20, 10 plus 8, 5, 4, 2, 1, but whether the sub-10 MHz values work was not checked.
- Whether CPU power-down in light sleep (CONFIG_PM_POWER_DOWN_CPU_IN_LIGHT_SLEEP=y) is actually used without esp_pm_configure(); cpu_domain_pd_allowed() needs CPU retention to be initialised. This changes light-sleep current, not behaviour.
- The real light-sleep and deep-sleep current of the whole gadget (SuperMini LDO, power LED, OLED module, SHT45, GY-273, button pull-ups). Only chip-only datasheet figures were gathered.
- The size of RC-slow-clock drift over a long light sleep or deep sleep at field temperatures, i.e. stopwatch or wall-clock error. The docs give no S3 figure.
- That FreeRTOS tick count is not compensated after manual light sleep (F23) is inferred from source (separate systimer counters, no vTaskStepTick). Not observed on hardware.
- That digital IO_MUX pull-ups (pinMode INPUT_PULLUP) stay active during light sleep in this build (F35) is inferred from sdkconfig and source. Not measured.
- Whether the OLED module's buttons have external pull-ups is unknown (BRIEF). If they do, the internal RTC pull-ups add only parallel current.

**Recommended snippets**

_pwr namespace: light sleep on buttons/timer, ext1 deep sleep on buttons, USB host gate (ESP32-S3, core 3.3.12)_ (compiled: yes)

```cpp
#include <Arduino.h>
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "driver/usb_serial_jtag.h"

namespace pwr {
// K1=GPIO4 K2=GPIO5 K3=GPIO6 K4=GPIO1, active-low; all are RTC IOs (S3 RTC IO = GPIO0..21)
constexpr gpio_num_t kButtons[] = {GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_1};
constexpr uint64_t kButtonMask = (1ULL << 4) | (1ULL << 5) | (1ULL << 6) | (1ULL << 1);

inline bool anyButtonDown() {
  for (gpio_num_t b : kButtons) if (gpio_get_level(b) == 0) return true;
  return false;
}

// True while a USB host sends SOF (PC/hub), false on a charger/power bank or no cable.
inline bool usbHostPresent() { return usb_serial_jtag_is_connected(); }

enum class Wake { Button, Timer, Other, Rejected, TooShort, Skipped };

// Light sleep until a button goes LOW or timeout_us elapses (0 = no timeout).
// RAM, peripherals, I2C state and GPIO config survive; esp_timer/millis are advanced by the slept time.
inline Wake lightSleep(uint64_t timeout_us) {
  if (anyButtonDown()) return Wake::Skipped;          // LOW-level source would fire at once
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL); // sources persist between sleeps otherwise
  for (gpio_num_t b : kButtons) gpio_wakeup_enable(b, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  if (timeout_us) esp_sleep_enable_timer_wakeup(timeout_us);
  Serial.flush();
  esp_err_t err = esp_light_sleep_start();
  for (gpio_num_t b : kButtons) {
    gpio_wakeup_disable(b);
    gpio_set_intr_type(b, GPIO_INTR_DISABLE);         // gpio_wakeup_enable() overwrote the intr type
  }
  if (err == ESP_ERR_SLEEP_TOO_SHORT_SLEEP_DURATION) return Wake::TooShort;
  if (err != ESP_OK) return Wake::Rejected;
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_GPIO:  return Wake::Button;
    case ESP_SLEEP_WAKEUP_TIMER: return Wake::Timer;
    default:                     return Wake::Other;
  }
}

// Deep sleep until any button LOW. Returns only if the sleep request was rejected.
inline esp_err_t deepSleepOnButtons(bool keepRtcPeriphOn) {
  while (anyButtonDown()) delay(10);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  for (gpio_num_t b : kButtons) {
    rtc_gpio_init(b);                                  // pad -> RTC IO mux
    rtc_gpio_set_direction(b, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(b);
    rtc_gpio_pullup_en(b);                             // RTC-IO pull-up (IO_MUX pull-up from pinMode does not apply here)
  }
  if (keepRtcPeriphOn) esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_sleep_enable_ext1_wakeup_io(kButtonMask, ESP_EXT1_WAKEUP_ANY_LOW);
  Serial.flush();
  esp_err_t err = esp_deep_sleep_try_to_start();       // no return on success
  for (gpio_num_t b : kButtons) pinMode(b, INPUT_PULLUP);   // gpio_config() calls rtc_gpio_deinit()
  return err;
}

// Call once in setup(), after pinMode(): which button woke us from deep sleep (-1 if none).
inline int deepSleepWakeButtonGpio() {
  if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT1) return -1;
  uint64_t m = esp_sleep_get_ext1_wakeup_status();
  return m ? __builtin_ctzll(m) : -1;
}
}  // namespace pwr

RTC_DATA_ATTR static uint32_t g_deepSleepCount = 0;

void setup() {
  Serial.begin(115200);
  for (gpio_num_t b : pwr::kButtons) pinMode(b, INPUT_PULLUP);
  int wakeGpio = pwr::deepSleepWakeButtonGpio();
  Serial.printf("wake gpio=%d deepSleeps=%lu\n", wakeGpio, (unsigned long)g_deepSleepCount);
}

void loop() {
  if (!pwr::usbHostPresent()) {
    int64_t t0 = esp_timer_get_time();
    pwr::Wake w = pwr::lightSleep(1000000ULL);
    Serial.printf("w=%d slept_us=%lld\n", (int)w, (long long)(esp_timer_get_time() - t0));
  }
  if (millis() > 600000UL) {
    g_deepSleepCount++;
    pwr::deepSleepOnButtons(false);
  }
  delay(10);
}
```

_Deep-sleep entry with ESP_ERROR_CHECK and the legacy-API probe (shows the ALL_LOW deprecation warning on S3)_ (compiled: yes)

```cpp
#include <Arduino.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"

static const gpio_num_t kButtons[] = {GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_6, GPIO_NUM_1};
static const uint64_t kButtonMask = (1ULL << 4) | (1ULL << 5) | (1ULL << 6) | (1ULL << 1);

static bool anyButtonDown() {
  for (gpio_num_t b : kButtons) if (gpio_get_level(b) == 0) return true;
  return false;
}

// Returns only if sleep was rejected (e.g. a button went LOW during entry).
static esp_err_t enterDeepSleepOnButtons(bool keepRtcPeriphOn) {
  while (anyButtonDown()) delay(10);                 // ANY_LOW would wake immediately otherwise
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  for (gpio_num_t b : kButtons) {
    ESP_ERROR_CHECK(rtc_gpio_init(b));               // route pad to RTC IO (as IDF example does)
    ESP_ERROR_CHECK(rtc_gpio_set_direction(b, RTC_GPIO_MODE_INPUT_ONLY));
    ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(b));
    ESP_ERROR_CHECK(rtc_gpio_pullup_en(b));          // RTC-IO pull-up, not the IO_MUX one
  }
  if (keepRtcPeriphOn) ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON));
  ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(kButtonMask, ESP_EXT1_WAKEUP_ANY_LOW));
  Serial.flush();
  return esp_deep_sleep_try_to_start();              // ESP_ERR_SLEEP_REJECT if a wake source is already active
}

void setup() {
  Serial.begin(115200);
  for (gpio_num_t b : kButtons) pinMode(b, INPUT_PULLUP);
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT1) {
    uint64_t m = esp_sleep_get_ext1_wakeup_status();
    Serial.printf("woke by mask 0x%llx\n", (unsigned long long)m);
  }
  // Deprecation probe: on S3 ALL_LOW is an alias of ANY_LOW and is marked deprecated.
  esp_err_t e = esp_sleep_enable_ext1_wakeup(kButtonMask, ESP_EXT1_WAKEUP_ALL_LOW);
  Serial.printf("legacy ext1 err=%d\n", (int)e);
  esp_sleep_disable_ext1_wakeup_io(0);
}

void loop() {
  if (millis() > 30000) {
    esp_err_t e = enterDeepSleepOnButtons(false);
    Serial.printf("deep sleep rejected: %d\n", (int)e);
    for (gpio_num_t b : kButtons) pinMode(b, INPUT_PULLUP);   // gpio_config() -> rtc_gpio_deinit()
  }
  delay(100);
}
```


## u8g2

| id | status | claim | method/conf | source |
|---|---|---|---|---|
| U1 | SINGLE-SOURCE | The constructor exists with signature U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C(const u8g2_cb_t *rotation, uint8_t reset = U8X8_PIN_NONE). It takes no SDA/SCL arguments. It calls u8g2_Setup_ssd1306_i2c_128x64_noname_f() with the byte callback u8x8_byte_arduino_2nd_hw_i2c and gpio callback u8x8_gpio_and_delay_arduino. | source-or-header/high | /mnt/wd/arduino-esp32/user/libraries/U8g2/src/U8g2lib.h:1700-1705 |
| U2 | SINGLE-SOURCE | CRITICAL: U8X8_HAVE_2ND_HW_I2C is only defined when the board defines WIRE_INTERFACES_COUNT > 1. arduino-esp32 3.3.12 defines WIRE_INTERFACES_COUNT nowhere: a recursive grep over the whole core directory found no hits. The ESP32 Wire.h decides on Wire1 with SOC_I2C_NUM instead. | source-or-header/high | /mnt/wd/arduino-esp32/user/libraries/U8g2/src/U8x8lib.h:134-141; grep -rln WIRE_INTERFACES_COUNT /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12 -> empty; Wire.h:148 '#if SOC_I2C_NUM > |
| U3 | SINGLE-SOURCE | CRITICAL, proven by a compile test: with the stock library on ESP32-S3, u8x8_byte_arduino_2nd_hw_i2c compiles to a stub that returns 1 and does no I/O. U8G2_*_2ND_HW_I2C therefore compiles and links cleanly, begin() and sendBuffer() 'succeed', and nothing is ever sent on Wire1. The result is a blank display with no error or log. In the test sketch, the U8X8_HAVE_2ND_HW_I2C #ifdef branch printed 'NOT defined'. | compile-test/high | compile test /mnt/wd/arduino-esp32/tmp/u8g2/staging/t1_2nd_hw_i2c.ino -> /mnt/wd/arduino-esp32/build/hwfacts-u8g2/t1.ino.elf (xtensa-esp-elf-objdump --disassemble=u8x8_byte_arduino_2nd_hw_i2c) |
| U4 | SINGLE-SOURCE | Compiling U8x8lib.cpp with the exact arduino-cli flags plus -DWIRE_INTERFACES_COUNT=2 turns the 2nd-HW-I2C callback into real code (136 bytes) that references Wire1. A sketch-folder build_opt.h is passed as '@build_opt.h' to every C/C++ compile, including libraries, so it could carry this define. A full sketch build with build_opt.h was NOT done; only the single object was compiled. | compile-test/medium | compile test /mnt/wd/arduino-esp32/tmp/u8g2/U8x8lib_wic2.o; /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/platform.txt:113,133,157 |
| U5 | SINGLE-SOURCE | If enabled, the stock 2nd-HW-I2C callback calls Wire1.begin() with NO pins at U8X8_MSG_BYTE_INIT, which happens inside u8g2.begin()/initDisplay(). On every START_TRANSFER it calls Wire1.setClock(u8x8->bus_clock), unless U8X8_DO_NOT_SET_WIRE_CLOCK is defined. When bus_clock is 0, it defaults to display_info->i2c_bus_clock_100kHz*100000. For ssd1306_128x64_noname that is 4*100000 = 400 kHz. | source-or-header/high | U8x8lib.cpp:1383-1416; clib/u8x8_d_ssd1306_128x64_noname.c:348 |
| U6 | SINGLE-SOURCE | In core 3.3.12, TwoWire::begin() with no pins does not clobber pins when the bus is already running. It logs a warning, returns true and keeps the earlier pins. Correct order is therefore Wire1.begin(10, 11, 400000) BEFORE u8g2.begin(). If Wire1 was not started and setPins() was not called, begin() fails for bus 1: the esp32s3 variant has no WIRE1_PIN_DEFINED/SDA1/SCL1 (only SDA=8, SCL=9). | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/libraries/Wire/src/Wire.cpp:299-303, 77-84; variants/esp32s3/pins_arduino.h:25-26 |
| U7 | SINGLE-SOURCE | Wire.setClock() maps to i2cSetClock(). That returns ESP_OK immediately when the frequency is unchanged, so u8g2's per-transfer setClock costs only a mutex take/give. Changing the frequency removes and re-adds every device handle on the bus. Core 3.3.12 uses the IDF i2c_master ('-ng') HAL when IDF >= 5.4; the bundled IDF is 5.5. | source-or-header/high | cores/esp32/esp32-hal-i2c-ng.c:17-19, 392-432; tools/esp32s3-libs/3.3.12/include/esp_common/include/esp_idf_version.h:14-16 |
| U8 | SINGLE-SOURCE | Recommended and compiled: put the display on Wire1 through a 15-line custom byte callback that uses Wire1 directly. It wraps u8g2_Setup_ssd1306_i2c_128x64_noname_f(), the same driver, CAD and buffer as the stock _F_ HW_I2C classes, in a U8G2 subclass. The callback does NOT begin the bus or change its clock, so the app owns Wire1.begin(10,11,400000). It also records endTransmission() errors so a missing display can be reported. Build result: 335,630 bytes flash, 25,000 bytes RAM. The final ELF shows calls to TwoWire::write and TwoWire::endTransmission on Wire1. | compile-test/high | /mnt/wd/arduino-esp32/tmp/u8g2/staging/t2.ino (compiled as /mnt/wd/arduino-esp32/tmp/u8g2/t1/t1.ino, log /mnt/wd/arduino-esp32/tmp/u8g2/t2.log) |
| U9 | SINGLE-SOURCE | First-bus HW_I2C on ESP32: when both clock and data pins were set through the constructor, BYTE_INIT calls Wire.begin(data, clock); otherwise it calls Wire.begin(), which on esp32s3 defaults to SDA=8/SCL=9. Separately, u8x8_gpio_and_delay_arduino(GPIO_AND_DELAY_INIT) runs pinMode(OUTPUT) on every pin index < 16 that is not NONE, including I2C_CLOCK (12) and I2C_DATA (13). In core 3.3.12, pinMode() on a pin owned by another bus calls perimanClearPinBus(), then i2cDetachBus(), then i2cDeinit(). Consequence: a HW_I2C constructor with explicit pins whose begin() runs AFTER that Wire bus was already started will tear the bus down. This is source analysis only; not run on hardware. | source-or-header/medium | U8x8lib.cpp:97-115, 1346-1355; clib/u8x8.h:320-326; cores/esp32/esp32-hal-gpio.c:122-127; cores/esp32/esp32-hal-i2c-ng.c:46-57 |
| U10 | SINGLE-SOURCE | U8G2::begin() = initDisplay() + clearDisplay() + setPowerSave(0). initDisplay() runs u8x8_d_helper_display_init(): reset high, delay(100), low, delay(100), high, delay(100). That is about 300 ms of blocking delay() even with U8X8_PIN_NONE as reset. The init sequence follows (16 I2C transactions), then clearDisplay() sends a full zero frame. beginSimple() does only initDisplay(). | source-or-header/high | U8g2lib.h:144-155; clib/u8x8_display.c:64-77; clib/u8x8_d_ssd1306_128x64_noname.c:335-346, 367-369; U8x8lib.cpp:135-136 |
| U11 | SINGLE-SOURCE | u8g2's SSD1306 128x64 'noname' init sequence: AE; D5 80; A8 3F; D3 00; 40; 8D 14 (charge pump on); 20 00 (horizontal addressing); A1; C8; DA 12; 81 CF (contrast 207); D9 F1 (pre-charge); DB 40 (VCOMH); 2E; A4; A6. A 'vcomh0' variant (u8g2_Setup_ssd1306_i2c_128x64_vcomh0_f) is also available. It gives the biggest setContrast() range but is noted to not work on all OLEDs. | source-or-header/high | clib/u8x8_d_ssd1306_128x64_noname.c:42-78; clib/u8g2_d_setup.c:379 |
| U12 | SINGLE-SOURCE | setPowerSave(1) sends only command 0xAE (display off/sleep). setPowerSave(0) sends 0xAF. GDDRAM content is kept, so no redraw is needed after wake. The datasheet says the charge pump is enabled only 'during display on'. It gives sleep currents ICC,SLEEP and IDD,SLEEP of max 10 uA each (display off, no panel attached). Whole-module current in sleep was not measured. | datasheet/high | clib/u8x8_d_ssd1306_128x64_noname.c:208-220, 269-276; Solomon Systech SSD1306 Rev 1.1, Table 13 DC characteristics p.48/59 and section 10.1.12 p.37/59; SSD1306 App Note Rev 0.4 p.3/6 (https://cdn-shop |
| U13 | SINGLE-SOURCE | setContrast(v) sends 0x81 followed by v (0..255). U8X8_WITH_SET_CONTRAST is defined by default. The SSD1306 has 256 contrast steps with a reset value of 0x7F; u8g2's init sets 0xCF. | datasheet/high | clib/u8x8_d_ssd1306_128x64_noname.c:289-295; clib/u8x8.h:93; SSD1306 datasheet p.28/59 and 10.1.7 p.36/59 |
| U14 | SINGLE-SOURCE | SSD1306 I2C address is 0b011110<SA0>, i.e. 7-bit 0x3C or 0x3D. u8g2 stores 8-bit addresses: the default set at CAD init is 0x78, and setI2CAddress() expects addr<<1 (0x78 or 0x7A) and must be called before begin(). Control byte: 0x00 = command stream, 0x40 = data stream (Co=0, D/C#=1). This is what u8g2 sends. | datasheet/high | SSD1306 datasheet 8.1.5 p.19-20/59; clib/u8x8_cad.c:474-480, 635-639 |
| U15 | SINGLE-SOURCE | SSD1306 I2C clock cycle time tcycle is at least 2.5 us, so the spec maximum is 400 kHz (Fast mode). Rates above 400 kHz are out of spec. | datasheet/high | SSD1306 datasheet Table 13-6 I2C Interface Timing Characteristics, p.54/59 |
| U16 | SINGLE-SOURCE | Measured on host with a counting byte callback and the same CAD (u8x8_cad_ssd13xx_fast_i2c): one full sendBuffer() = 64 I2C transactions. Per page that is 1 command transaction with 0x00,0x40; 1 with 0x00,0x10\|x>>4,x&15,0xB0\|page; and 6 data transactions (5x24 + 1x8 bytes, each with a leading 0x40). Total 1120 payload + 64 address = 1184 bytes on the wire. At 9 clocks/byte that is a theoretical minimum of about 26.8 ms at 400 kHz (about 37 fps max) and 107 ms at 100 kHz. Max payload per transaction is 25 bytes, which fits the 128-byte ESP32 Wire buffer. Real ESP32-S3 time, including IDF i2c_master per-transaction overhead, was NOT measured; the recommended sketch prints it. | host-run/high | host program /mnt/wd/arduino-esp32/tmp/u8g2host/count_i2c.cpp (run output); clib/u8x8_cad.c:626-632; clib/u8x8_d_ssd1306_128x64_noname.c:297-326 |
| U17 | SINGLE-SOURCE | updateDisplayArea(tx,ty,tw,th) works in 8x8 tiles (tx 0..15, ty 0..7) and only in full-buffer (_F_) mode. Measured: (0,2,16,4) = 32 transactions / 592 bytes, at least 13.4 ms at 400 kHz. (4,3,8,2) = 10 transactions / 156 bytes, at least 3.5 ms. This is useful for a stopwatch line that refreshes often. | host-run/high | clib/u8g2_buffer.c:153-175; host run count_i2c.cpp; u8g2 wiki u8g2reference |
| U18 | SINGLE-SOURCE | Frame buffer layout for _F_ SSD1306 128x64: 1024 bytes, 8 pages x 128 columns. The byte for pixel (x,y) is buf[(y>>3)*128 + x], bit (y&7), with LSB = top row of the page. This matches the SSD1306 GDDRAM (D0 = top row), so the buffer is sent unchanged. Get it with getBufferPtr(), getBufferTileWidth()=16 and getBufferTileHeight()=8. The buffer is a function-static array in u8g2_m_16_8_f(), so all instances with that setup share it. | source-or-header/high | clib/u8g2_ll_hvline.c:88; clib/u8x8_capture.c:45-57; clib/u8g2_d_memory.c:61-70; SSD1306 datasheet 8.7 p.25/59 |
| U19 | SINGLE-SOURCE | Host rendering works with NO hardware: the u8g2 C core (src/clib/*.c, 136 files) builds with gcc into libu8g2.a in about 25 s wall time (parallel). U8g2lib.h (the C++ U8G2 class) compiles with g++ -std=c++17 on Linux without ARDUINO defined. A U8G2 subclass calling u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_empty, u8x8_dummy_cb) renders into the same buffer as the firmware. A hand-written PBM dump and the library's u8g2_WriteBufferPBM() produced identical 128x64 images. | host-run/high | /mnt/wd/arduino-esp32/tmp/u8g2host/build.sh, host_render.cpp; outputs out_manual.pbm, out_u8g2.pbm, out_x4.png |
| U20 | SINGLE-SOURCE | On host, the Arduino Print-based API is not available: print(), and writeBufferPBM(Print&) under #ifdef ARDUINO. UI code shared between firmware and host renderer should use drawStr()/drawUTF8() with snprintf() and take a U8G2& reference. The C function u8g2_WriteBufferPBM(u8g2_t*, void(*out)(const char*)) works on both. | source-or-header/high | U8g2lib.h:314-322, 415-418; U8x8lib.h:190-192; clib/u8g2.h:1821 |
| U21 | SINGLE-SOURCE | setDrawColor: 0 clears, 1 sets, 2 XOR. It is implemented as or_mask (color<=1) and xor_mask (color!=1) in the low-level hvline. clear()/clearBuffer() always write 0. XOR is not supported for drawCircle/drawDisc/ellipses (wiki). Host test: a XOR circle lights 48 px vs 56 px normally (overlapping points cancel). The wiki advises not combining solid font mode with XOR. | host-run/high | clib/u8g2_ll_hvline.c:94-99; https://github.com/olikraus/u8g2/wiki/u8g2reference (setDrawColor); host run neg.cpp |
| U22 | SINGLE-SOURCE | setFontMode(0) (solid, default) overwrites the glyph background. setFontMode(1) (transparent) does not. Host test on a filled screen: mode 0 'A' (6x10) leaves 8173/8192 lit, mode 1 leaves 8192/8192, and mode 1 with drawColor 0 draws black text (8176/8192 lit). Default font reference position is baseline (setFontPosBaseline). setFontPosTop/Center/Bottom shift y. | host-run/high | https://github.com/olikraus/u8g2/wiki/u8g2reference (setFontMode, setFontPosBaseline); host run neg.cpp |
| U23 | SINGLE-SOURCE | drawXBM(x,y,w,h,bits): XBM rows of ceil(w/8) bytes, LSB of each byte = leftmost pixel. setBitmapMode(0) (default, solid) also paints 0-bits in the inverse color. setBitmapMode(1) makes 0-bits transparent. On ESP32 (and host), u8x8_pgm_read is a plain pointer dereference, so drawXBM and drawXBMP behave identically and no PROGMEM is needed. | source-or-header/high | clib/u8g2_bitmap.c:107-185; clib/u8x8.h:154-180 |
| U24 | SINGLE-SOURCE | setClipWindow(x0,y0,x1,y1): x0/y0 inclusive, x1/y1 EXCLUSIVE. Host test: clip(10,10,20,20) plus a full-screen box gives exactly 100 lit pixels. setMaxClipWindow() resets it. | host-run/high | https://github.com/olikraus/u8g2/wiki/u8g2reference (setClipWindow); clib/u8g2_setup.c:56-63; host run neg.cpp |
| U25 | SINGLE-SOURCE | getStrWidth() is ASCII/8-bit only; getUTF8Width()/drawUTF8() decode UTF-8. Glyphs missing from the current font are silently skipped. Host test: drawUTF8("Ж") with u8g2_font_6x10_tf draws 0 px and the width is 0, with no error. Use u8g2_IsGlyph(u8g2_t*, uint16_t) to check coverage. enableUTF8Print() only affects Arduino print(). | host-run/high | https://github.com/olikraus/u8g2/wiki/u8g2reference (getStrWidth/getUTF8Width/enableUTF8Print); host run neg.cpp |
| U26 | SINGLE-SOURCE | getAscent()/getDescent() use the reference height set by setFontRefHeightText(), which is the default: ascent of 'A' or '1'. Descent is negative. getMaxCharHeight() is the font bounding-box height and can be much larger than ascent-descent for _all fonts (tiny5_t_all: maxH 16, ascent 5, descent -1). | host-run/high | https://github.com/olikraus/u8g2/wiki/u8g2reference (setFontRefHeightText); host table /mnt/wd/arduino-esp32/tmp/u8g2host/fontscan.tsv |
| U27 | SINGLE-SOURCE | BUG/PITFALL: u8g2_DrawLine uses unsigned u8g2_uint_t (uint16_t, because U8G2_16BIT is unconditionally defined in 2.36.19) and has no clipping. Negative endpoints therefore draw garbage. Host: drawLine(-10,-10,10,10) lit 54 px starting at (10,10), where 11 were expected; drawLine(5,-20,60,40) drew a vertical stroke. drawBox and drawStr with negative x clip correctly: drawBox(-3,0,6,4) gives 12 px, drawStr(-6,..,"AB") gives the same as "B" at 0. A signed Bresenham helper that drops off-screen pixels gives 11 px. | host-run/high | clib/u8g2_line.c (u8g2_DrawLine); clib/u8g2.h:71; host runs neg.cpp, neg2.cpp, safeline.cpp |
| U28 | SINGLE-SOURCE | Font suffix meaning (u8g2 wiki). Purpose letter: t = transparent, h = common height, m = monospace, 8 = fits an 8x8 box. Charset letter: f = up to 256 glyphs, r = ASCII 32..127, u = 32..95, n = digits plus date/time glyphs; _t_cyrillic / _t_all are custom sets. In u8g2.h 2.36.19 all 2043 'extern const uint8_t u8g2_font_*' fonts linked and were scanned on host. | host-run/high | https://github.com/olikraus/u8g2/wiki/fntlist8 ('U8g2 Font Names'); /mnt/wd/arduino-esp32/tmp/u8g2host/fontscan.tsv (2043 rows) |
| U29 | SINGLE-SOURCE | Big-digit fonts, all verified to exist (host-measured: digit advance, ascent/descent, flash bytes). MONOSPACE digits, no jitter when counting: logisoso16_tn (10px, asc16, 287B), logisoso24_tn (16px, asc24/desc-6, '00:00'=70px, '00:00.0'=94px, 421B), logisoso28_tn (18px, asc28, 427B), logisoso42_tn (27px, asc42, 724B); inb16/19/24/27/30_mn (14/15/21/22/24px; ':' is a full cell); profont22_mn (12px, asc14), profont29_mn (16px, asc19); 7Segments_26x42_mn (32px cell incl. ':', asc42, 842B, only 4 cells fit across 128px); freedoomr25_mn (19px). PROPORTIONAL digits, '1' differs: logisoso20_tn, logisoso32_tn, logisoso38_tn, fub20/25/30_tn, helvB18/24_tn, crox5hb_tn. | host-run/high | host run /mnt/wd/arduino-esp32/tmp/u8g2host/digits.cpp; nm -S build/obj/u8g2_fonts.o |
| U30 | SINGLE-SOURCE | Small text fonts (maxH/ascent/descent): 4x6_tf 6/5/-1; 5x7_tf 7/6/-1 (1612B); 5x8_tf 8/6/-1; 6x10_tf 10/7/-2 (2000B); 6x12_tf 12/7/-2; 6x13_tf 13/9/-2; 7x13_tf 13/9/-2; 7x14_tf 14/10/-2; 8x13_tf 13/9/-2; profont10/11/12/15/17/22_tf; helvB08/10/12/14/18/24_tf; helvR08..24_tf; t0_11..t0_16 variants; tom_thumb_4x6_tf 6/5/-1; tiny5_tf 5px ascent; micro_tr 5px. All the _tf fonts listed contain the degree sign (0xB0, Latin-1). | host-run/high | host table /mnt/wd/arduino-esp32/tmp/u8g2host/fontscan.tsv (columns maxH, ascent, descent, deg) |
| U31 | SINGLE-SOURCE | Fonts with Cyrillic, verified with u8g2_IsGlyph for U+0410 'А', U+044F 'я', U+0401 'Ё'. *_t_cyrillic (maxH/ascent/descent, flash): 4x6 (6/5/-1, 1558B), 5x7 (7/6/-1, 1934B), 5x8, 6x12 (12/7/-2, 4690B), 6x13, 6x13B, 7x13, 8x13 (13/9/-2, 5143B), 9x15, 10x20 (20/13/-4, 6979B), cu12, unifont (16/10/-2, 6659B), haxrcorp4089 (11/7/-2; NO 'Ё'), inr24/27/30/33/38/42/46/49/53 (inr24: 38/25/-6, 6768B). *_all fonts with Cyrillic: tiny5_t_all, tiny5duo_t_all, t0_11_t_all (44301B), glasstown/shylock/mercutio_basic/mercutio_sc/nine_by_five/rosencrantz/guildenstern _nbp_t_all, Born2bSportySlab_t_all, pxplustandynewtv_t_all/_8_all, pxplusibmvga8/9 _t_all/_m_all, boutique_bitmap_7x7/9x9 _t_all, plus the CJK unifont/wqy gb2312 fonts. | host-run/high | host table /mnt/wd/arduino-esp32/tmp/u8g2host/fontscan.tsv; nm -S sizes |
| U32 | SINGLE-SOURCE | PITFALL: none of the *_t_cyrillic fonts contain the degree sign U+00B0. drawUTF8("23°") with them silently drops the '°'. Fonts with both Cyrillic and '°': tiny5_t_all, tiny5duo_t_all, t0_11_t_all, the *_nbp_t_all family, Born2bSportySlab_t_all, pxplus*_all. Otherwise draw '°' with a Latin-1 _tf font (e.g. 6x10_tf) or as a drawCircle(r=1). | host-run/high | host table /mnt/wd/arduino-esp32/tmp/u8g2host/fontscan.tsv (deg column); specimen /mnt/wd/arduino-esp32/tmp/u8g2host/specimen.png |
| U33 | SINGLE-SOURCE | ESP32 core 3.3.12 Wire.endTransmission() return codes: 0 = OK, 2 = ESP_FAIL/ESP_ERR_NOT_FOUND (NACK), 5 = timeout, 4 = other. The default transaction timeout is 50 ms. The Wire TX/RX buffer defaults to 128 bytes (I2C_BUFFER_LENGTH). | source-or-header/high | libraries/Wire/src/Wire.cpp:445-476; Wire.h:48-49, 128 |
| U34 | SINGLE-SOURCE | Default pins for Wire (bus 0) on the 'ESP32S3 Dev Module' variant are SDA=8, SCL=9. The sensor bus (SDA=8, SCL=7) must therefore be started explicitly with Wire.begin(8, 7, freq). Wire.begin() without arguments would use SCL=9. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/variants/esp32s3/pins_arduino.h:25-26; Wire.cpp:70-76 |

**Pitfalls**

- DO NOT use U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C on arduino-esp32 3.3.12 + U8g2 2.36.19. It compiles but its byte callback is a no-op that returns 1, so the display stays blank and nothing reports an error. The ESP32 core never defines WIRE_INTERFACES_COUNT. Proven by disassembly of the built ELF (fact U3).
- A '#define WIRE_INTERFACES_COUNT 2' inside the .ino does NOT fix it, because U8x8lib.cpp is a separate translation unit. Only a global flag works, e.g. sketch build_opt.h with -DWIRE_INTERFACES_COUNT=2 (object-level verified only). That route is fragile: Arduino IDE build caching, the define is global for all libraries, and every transfer calls Wire1.setClock().
- Recommended instead: a custom u8x8 byte callback on Wire1 inside a small U8G2 subclass (snippet 1). The app calls Wire1.begin(10, 11, 400000) BEFORE u8g2.begin() and owns the bus clock.
- U8G2_..._HW_I2C(rot, reset, clock, data) on ESP32: u8g2.begin() runs pinMode(OUTPUT) on the SDA/SCL pins before Wire.begin(data, clock). If that Wire bus was already started (e.g. by sensor code), core 3.x pinMode() deinitializes the I2C bus through the peripheral manager. Source analysis, not run on hardware.
- Wire.begin() with no arguments on esp32s3 uses SDA=8/SCL=9, not the sensors' SCL=7. Always pass the pins.
- u8g2.begin() blocks for about 300 ms (three delay(100) in the reset helper, even without a reset pin), then sends init and a full clear frame. Do it once in setup(), not on every wake. After setPowerSave(1), use setPowerSave(0) to wake; RAM content is preserved.
- Full-frame sendBuffer() is at least 26.8 ms at 400 kHz (64 transactions, 1184 bytes on the wire). Do not call it from an ISR or a time-critical loop. For a fast stopwatch line, use updateDisplayArea() in 8x8 tile units. That function does nothing unless you use the _F_ (full buffer) setup.
- The SSD1306 spec maximum is 400 kHz (tcycle >= 2.5 us). Higher rates are out of spec. The bus is private to the OLED, so per-transfer setClock in stock callbacks would not disturb the sensors anyway.
- u8g2 drawLine() breaks with negative or off-screen coordinates: unsigned 16-bit math and no clipping. Keep compass-needle and animation endpoints inside 0..127/0..63, or use the signed clipped helper (snippet 3). drawBox, drawStr and drawXBM clip negative x correctly.
- XOR (setDrawColor(2)) is not supported for drawCircle/drawDisc/ellipse, which leaves visible holes. Avoid XOR together with solid font mode (setFontMode(0)). For text on a filled background, use setFontMode(1) with drawColor 0 or 2.
- Glyphs missing from a font are silently skipped by drawUTF8/getUTF8Width (width 0). None of the *_t_cyrillic fonts have '°'. haxrcorp4089_t_cyrillic lacks 'Ё'. Use u8g2_IsGlyph() in host tests to assert every UI string is fully covered by its font.
- getStrWidth() is for ASCII only. Use getUTF8Width() for any string containing Cyrillic. The .ino/.cpp source must be saved as UTF-8.
- Proportional digit fonts (logisoso20/32/38_tn, fub*_tn, helvB*_tn) make a running clock jitter as '1' changes width. Pick a MONO font (logisoso16/24/28/42_tn, inb*_mn, profont*_mn) or draw each digit in a fixed-width cell.
- setI2CAddress() takes the 8-bit form (0x3C<<1 = 0x78; 0x7A for 0x3D) and must be called before begin().
- Host-shared UI code must not use u8g2.print()/setCursor-print. Print is Arduino-only, so on Linux the U8G2 class has no Print base. Use snprintf + drawStr/drawUTF8 and pass U8G2& to the view code.
- u8g2 is not thread-safe, and the _F_ frame buffer is a single static 1024-byte array. Draw and send from one FreeRTOS task only.
- The _t_all fonts are large: t0_11_t_all is 44 KB, tiny5_t_all 17 KB. Prefer the *_t_cyrillic fonts (1.5-7 KB) unless '°' or other symbols are needed from the same font.

**Unverified (researcher)**

- Actual sendBuffer() duration on the ESP32-S3 with the IDF 5.5 i2c_master driver (64 transactions/frame, per-transaction overhead) and the real SCL frequency at 400 kHz with the module's pull-ups. Only the theoretical minimum of 26.8 ms was derived. The recommended sketch prints the measured micros() value for checking on hardware.
- Full sketch build and on-hardware behaviour of the build_opt.h (-DWIRE_INTERFACES_COUNT=2) route. Only U8x8lib.cpp was recompiled with the define. Arduino IDE rebuild-on-change of build_opt.h was not checked.
- That the OLED module answers at 0x3C and is a genuine SSD1306. 0.96" modules are sometimes SSD1315 or SH1106; SH1106 needs a different u8g2 setup (132-column RAM). Not verified on hardware.
- The pinMode-induced I2C bus deinit (fact U9) when HW_I2C with explicit pins runs after Wire was already begun. Derived from source, not run on hardware.
- Whole-module current with setPowerSave(1). The datasheet gives only the controller limit of ≤10 µA ICC and IDD each, no panel. The module's regulator, pull-ups and boost circuit were not measured.
- Visible effect of setContrast() and of the vcomh0 init variant on this specific panel, and whether U8G2_R0 or U8G2_R2/setFlipMode(1) matches how the module is mounted.
- Battery-icon semantics of u8g2_font_battery19_tn glyphs: only the existence of glyphs '0'..'7' was confirmed. Open Iconic glyph code meanings were not verified.
- Pixel-exact match between host render and the physical panel. The buffer bytes are identical by construction (same setup function and buffer layout), but no photo comparison was made.

**Recommended snippets**

_ESP32-S3: SSD1306 128x64 on Wire1 (SDA=10, SCL=11) via custom u8x8 byte callback - compiled with arduino-cli esp32:esp32:esp32s3:CDCOnBoot=cdc (core 3.3.12, U8g2 2.36.19). File: /mnt/wd/arduino-esp32/tmp/u8g2/staging/t2.ino_ (compiled: yes)

```cpp
#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

static constexpr int      kOledSda   = 10;
static constexpr int      kOledScl   = 11;
static constexpr uint32_t kOledI2cHz = 400000;   // SSD1306 spec max (tcycle >= 2.5 us)
static constexpr uint8_t  kOledAddr7 = 0x3C;     // 0x3D if SA0 strapped high

static TwoWire& oledBus = Wire1;
static volatile uint8_t oledLastErr = 0;          // last non-zero endTransmission() code
static volatile uint32_t oledErrCount = 0;

extern "C" uint8_t u8x8_byte_wire1(u8x8_t* u8x8, uint8_t msg, uint8_t arg_int, void* arg_ptr) {
  switch (msg) {
    case U8X8_MSG_BYTE_INIT:          // bus is started by app code (oledBus.begin) before u8g2.begin()
    case U8X8_MSG_BYTE_SET_DC:
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      oledBus.beginTransmission(u8x8_GetI2CAddress(u8x8) >> 1);
      break;
    case U8X8_MSG_BYTE_SEND:
      oledBus.write(static_cast<const uint8_t*>(arg_ptr), arg_int);
      break;
    case U8X8_MSG_BYTE_END_TRANSFER: {
      uint8_t e = oledBus.endTransmission();   // 0 ok, 2 NACK, 4 other, 5 timeout
      if (e) { oledLastErr = e; oledErrCount++; }
      break;
    }
    default:
      return 0;
  }
  return 1;
}

class U8G2_SSD1306_128X64_F_WIRE1 : public U8G2 {
 public:
  explicit U8G2_SSD1306_128X64_F_WIRE1(const u8g2_cb_t* rotation) : U8G2() {
    // same driver/CAD/buffer as U8G2_SSD1306_128X64_NONAME_F_*_HW_I2C
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, rotation, u8x8_byte_wire1, u8x8_gpio_and_delay_arduino);
  }
};

U8G2_SSD1306_128X64_F_WIRE1 u8g2(U8G2_R0);

bool oledProbe() {
  oledBus.beginTransmission(kOledAddr7);
  return oledBus.endTransmission() == 0;
}

void setup() {
  Serial.begin(115200);
  oledBus.begin(kOledSda, kOledScl, kOledI2cHz);   // 1) bus first, with our pins and clock
  if (!oledProbe()) {
    Serial.printf("OLED not found at 0x%02X on Wire1 SDA=%d SCL=%d: check wiring/power/address\n",
                  kOledAddr7, kOledSda, kOledScl);
  }
  u8g2.setI2CAddress(kOledAddr7 << 1);             // 8-bit form expected by u8g2
  u8g2.begin();                                      // 2) init seq + clear + display on (~300 ms of delays)
  u8g2.setContrast(128);
  u8g2.enableUTF8Print();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x12_t_cyrillic);
  u8g2.drawUTF8(0, 12, "Привет");
  u8g2.setFont(u8g2_font_logisoso24_tn);
  u8g2.drawStr(0, 50, "12:34");
  uint32_t t0 = micros();
  u8g2.sendBuffer();
  uint32_t dt = micros() - t0;
  Serial.printf("sendBuffer: %lu us, i2c errors: %lu (last %u)\n", (unsigned long)dt,
                (unsigned long)oledErrCount, oledLastErr);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 1000) {
    last = millis();
    u8g2.setDrawColor(0); u8g2.drawBox(0, 52, 128, 12); u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(0, 62); u8g2.print(millis() / 1000);
    u8g2.updateDisplayArea(0, 6, 16, 2);   // tiles: rows 6..7 = pixels y 48..63 only
  }
}
```

_Host (Linux x86, g++ 13): build u8g2 core and render the SAME U8G2 class into memory, dump PBM and convert to PNG. Built and run in /mnt/wd/arduino-esp32/tmp/u8g2host (build.sh + host_render.cpp); output out_x4.png_ (compiled: yes)

```cpp
# ---- build.sh ----
#!/bin/bash
set -e
U8G2=/mnt/wd/arduino-esp32/user/libraries/U8g2/src
OUT=/mnt/wd/arduino-esp32/tmp/u8g2host/build
mkdir -p $OUT/obj
for f in $U8G2/clib/*.c; do           # C core -> static lib; fonts get own sections
  o=$OUT/obj/$(basename ${f%.c}).o
  [ -f $o ] || gcc -c -O2 -ffunction-sections -fdata-sections -I$U8G2/clib "$f" -o "$o" &
done
wait
ar rcs $OUT/libu8g2.a $OUT/obj/*.o
g++ -std=c++17 -O2 -Wall -I$U8G2 -I$U8G2/clib host_render.cpp -L$OUT -lu8g2 -Wl,--gc-sections -o $OUT/host_render
# run:  cd /mnt/wd/arduino-esp32/tmp/u8g2host && TMPDIR=/mnt/wd/arduino-esp32/tmp ./build.sh && ./build/host_render

// ---- host_render.cpp (essentials) ----
#include <cstdio>
#include "U8g2lib.h"          // compiles without ARDUINO; no Print/print() on host
class U8G2_HOST_128X64 : public U8G2 {
 public:
  U8G2_HOST_128X64() : U8G2() {
    // same driver + buffer layout as firmware; byte/gpio callbacks swallow everything
    u8g2_Setup_ssd1306_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_empty, u8x8_dummy_cb);
  }
};
// page layout: byte = buf[(y/8)*128 + x], bit (y&7), LSB = top
static int pixel(const uint8_t* buf, int x, int y) { return (buf[(y >> 3) * 128 + x] >> (y & 7)) & 1; }
static void dump_pbm(U8G2& g, const char* path) {
  const uint8_t* buf = g.getBufferPtr();
  const int w = g.getBufferTileWidth() * 8, h = g.getBufferTileHeight() * 8;   // 128 x 64
  FILE* f = fopen(path, "w"); fprintf(f, "P1\n%d %d\n", w, h);
  for (int y = 0; y < h; y++) { for (int x = 0; x < w; x++) fputc(pixel(buf, x, y) ? '1' : '0', f); fputc('\n', f); }
  fclose(f);
}
static FILE* g_pbm2; static void out_cb(const char* s) { fputs(s, g_pbm2); }
int main() {
  U8G2_HOST_128X64 g;
  g.begin();                                   // all I2C traffic goes to u8x8_byte_empty
  g.clearBuffer();
  g.setFont(u8g2_font_logisoso24_tn); g.drawStr(0, 26, "12:34");
  g.setFont(u8g2_font_6x12_t_cyrillic); g.drawUTF8(0, 54, "Привет, поход!");
  g.setDrawColor(2); g.drawBox(96, 0, 32, 20); g.setFontMode(1);
  g.setFont(u8g2_font_5x7_tf); g.drawStr(100, 12, "XOR"); g.setDrawColor(1);
  dump_pbm(g, "out_manual.pbm");
  g_pbm2 = fopen("out_u8g2.pbm", "w"); u8g2_WriteBufferPBM(g.getU8g2(), out_cb); fclose(g_pbm2);  // library writer, identical
}

# ---- PBM -> PNG (python3 + PIL), lit OLED pixel = white ----
from PIL import Image
img = Image.open('out_manual.pbm').convert('L').point(lambda v: 255 - v)
img.resize((512, 256), Image.NEAREST).save('out_x4.png')
```

_Signed, clipped line helper (u8g2 drawLine breaks on negative coords) - compiled and tested on host (safeline.cpp): (-10,-10)-(10,10) -> 11 px; in-range result identical to drawLine_ (compiled: yes)

```cpp
#include <cstdlib>
// Signed Bresenham; drops off-screen pixels. Use for compass needle / sliding-screen animations.
static void drawLineSafe(U8G2& g, int x0, int y0, int x1, int y1) {
  const int W = g.getDisplayWidth(), H = g.getDisplayHeight();
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    if (x0 >= 0 && x0 < W && y0 >= 0 && y0 < H) g.drawPixel(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}
```

_Host test helper: assert a UTF-8 UI string is fully covered by the current font (avoids silently dropped glyphs such as '°' in *_t_cyrillic) - pattern used in fontscan.cpp/neg.cpp; this exact function not separately compiled_ (compiled: no)

```cpp
// returns false if any code point of utf8 is missing in the current font
static bool fontCovers(U8G2& g, const char* s) {
  u8x8_t* u8x8 = u8g2_GetU8x8(g.getU8g2());
  u8x8_utf8_init(u8x8);
  for (; *s; ++s) {
    uint16_t e = u8x8_utf8_next(u8x8, (uint8_t)*s);
    if (e == 0x0fffe) continue;            // need more bytes
    if (e == 0x0ffff) break;               // end
    if (!u8g2_IsGlyph(g.getU8g2(), e)) return false;
  }
  return true;
}
```

_Alternative (NOT recommended, object-level verified only): enable the stock 2ND_HW_I2C path via sketch build_opt.h_ (compiled: no)

```cpp
// <sketch>/build_opt.h  (one line; passed as @build_opt.h to every C/C++ compile incl. libraries)
-DWIRE_INTERFACES_COUNT=2

// sketch: bus MUST be started with pins first; u8g2's BYTE_INIT then calls Wire1.begin() which
// returns early ("Bus already started in Master Mode.") and keeps pins 10/11.
U8G2_SSD1306_128X64_NONAME_F_2ND_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
void setup() {
  Wire1.begin(10, 11, 400000);
  u8g2.setBusClock(400000);   // u8g2 calls Wire1.setClock(bus_clock) on every transfer
  u8g2.begin();
}
```


## supermini

| id | status | claim | method/conf | source |
|---|---|---|---|---|
| SM-01 | SINGLE-SOURCE | The AITEWIN 'ESP32 SuperMini' in the listing photo looks like the same PCB as the nologo.tech ESP32-S3 SuperMini. Both have the same edge pin order, the same back-pad layout (14-18,21,33-38 / 39-42,45-48), B+/B- pads, a 'BAT' LED and a red 'C3' ceramic antenna. The vendor names the chip ESP32-S3FH4R2, and the vendor schematic (dated 10/08/2024) labels U1 'ESP32-S3FH4R2'. This is a visual match only: the listing does not state the chip. | secondary/medium | esp32-s3-supermini_listing.png (listing photo, not in the public repo) (zoomed); https://www.nologo.tech/product/esp32/esp32s3/esp32s3supermini/esp32S3SuperMini.html ; schematic https://www.nologo.tech/assets/img/esp32/esp32s3 |
| SM-02 | SINGLE-SOURCE | ESP32-S3FH4R2 has 4 MB in-package Quad-SPI flash and 2 MB in-package Quad-SPI PSRAM. It is rated -40..85 C, VDD_SPI is 3.3 V, and chip revisions v0.1/v0.2 exist. In-package flash runs at 80 MHz max by default; 120 MHz requires contacting Espressif. | datasheet/high | ESP32-S3 Series Datasheet v2.2, Table 1-1 + footnote 2 (https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) |
| SM-03 | SINGLE-SOURCE | The in-package flash/PSRAM uses SPICS1 (GPIO26, PSRAM CE#), SPIHD, SPIWP, SPICS0, SPICLK, SPIQ and SPID (GPIO27-32). These pins must not be used. GPIO33-37 are taken only by octal (8-line) memory, so on the quad FH4R2 they are free (all exposed as back pads). | datasheet/high | ESP32-S3 datasheet v2.2, Table 2-14 and Section 2.3.5 (P3/P4 lists) |
| SM-04 | SINGLE-SOURCE | Exposed pins. Left edge (front): TX=GPIO43, RX=GPIO44, then 1,2,3,4,5,6,7. Right edge: 5V(=VBUS), GND, 3V3(OUT), 13,12,11,10,9,8. Back pads: 14,15,16,17,18,21,33,34,35,36,37,38 and 39,40,41,42,45,46,47,48, plus B+ / B- (battery) and a 'BOOST' solder jumper. GPIO19/20 (USB) are not brought out. The vendor schematic shows no other on-board load on the pins the project uses (1,4,5,6,7,8,10,11,12); they go only to the headers. | secondary/medium | listing photo (back side zoom); nologo schematic blocks 'CONNECT' (H1: GP7..GP1,U0RXD,U0_TXD; H2: GP8..GP13,3V3,GND,VBUS) and pads list; vendor pinout image .../esp32s3foot1.jpg |
| SM-05 | SINGLE-SOURCE | GPIO1 is a normal GPIO, not USB. It is chip pin 6 = GPIO1 / RTC_GPIO1 / TOUCH1 / ADC1_CH0 and is classed Priority-2 ('can be freely used without restrictions'). At reset it is input-enabled with no pull, so the K4 button needs INPUT_PULLUP. The USB pins are GPIO19 (D-) and GPIO20 (D+). | source-or-header/high | ESP32-S3 datasheet v2.2 pin table rows 6/25/26, Table 2-9; IDF v5.5.5 header soc/esp32s3/include/soc/usb_pins.h |
| SM-06 | SINGLE-SOURCE | There are four strapping pins, all latched at reset (hold time tH >= 3 ms after CHIP_PU high) and free for normal use afterwards. GPIO0: weak pull-up, sets boot mode (1 = SPI boot). GPIO3: floating, selects JTAG source, but only matters if EFUSE_STRAP_JTAG_SEL is burned (default 0, so ignored). GPIO45: weak pull-down, selects VDD_SPI; a 1 at reset with default eFuses gives 1.8 V, which would break the 3.3 V FH4R2 flash, so it must stay low at reset. GPIO46: weak pull-down, used for boot mode together with GPIO0 and for ROM message printing. | datasheet/high | ESP32-S3 datasheet v2.2 Section 3, Tables 3-1..3-5 |
| SM-07 | SINGLE-SOURCE | Download mode: holding GPIO0=0 (with GPIO46=0) at reset gives Joint Download Boot (USB-Serial-JTAG, USB-OTG or UART). On this board BOOT connects GPIO0 to GND and RESET connects CHIP_PU to GND (10k pull-up, 1uF). Manual procedure: hold BOOT, press and release RESET, release BOOT (or hold BOOT while plugging in USB). The vendor says that after flashing in manual download mode you press RESET once to run the program. Normally no buttons are needed: the USB Serial/JTAG controller puts the chip into download mode automatically. | official-docs/high | Datasheet Table 3-3; nologo page; nologo schematic (K1 BOOT, K2 RESET, R2 10K, C2 1uF); ESP-IDF v5.5 USB Serial/JTAG console guide (https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32s3/ |
| SM-08 | SINGLE-SOURCE | USB Serial/JTAG limits: in deep sleep the COM port disappears, and in light sleep the host may see the device as disconnected and may need a replug. If firmware reconfigures GPIO19/20 or disables the controller, the port disappears and flashing needs a manual download mode (GPIO0 low + reset). | official-docs/high | ESP-IDF release-v5.5 USB Serial/JTAG console guide, Limitations |
| SM-09 | SINGLE-SOURCE | LEDs per the vendor schematic. GPIO48 drives BOTH a red 0402 LED (GPIO48 -> R1 1k -> LED -> GND, active HIGH, about 1.5 mA) AND the DIN of a WS2812 (VDD on 3V3; DIN 10k pull-up WSR1 not fitted). The blue LED is the charger CHRG indicator (VBUS -> 1k -> LED -> TP4054 CHRG) and is not software-controllable. There is no separate always-on power LED in the schematic. Note: the vendor text says 'RGB and blue LED share GPIO48', but its own schematic and pinout image ('LED GP48', 'BATTERY LED') show the red LED on GPIO48 and the blue LED on the charger. | secondary/medium | nologo schematic LED/CHARGE blocks; nologo page; vendor pinout image |
| SM-10 | SINGLE-SOURCE | Arduino core 3.3.12, variant esp32s3 (ESP32S3 Dev Module): PIN_RGB_LED=48, LED_BUILTIN=SOC_GPIO_PIN_COUNT+48 (a virtual RGB pin; SOC_GPIO_PIN_COUNT=49), RGB_BRIGHTNESS=64, TX=43, RX=44, default SDA=8, SCL=9. The API is rgbLedWrite(pin,r,g,b) (default order LED_COLOR_ORDER_GRB) and rgbLedWriteOrdered(); neopixelWrite() is deprecated. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/variants/esp32s3/pins_arduino.h; cores/esp32/esp32-hal-rgb-led.h |
| SM-11 | SINGLE-SOURCE | 3V3 regulator: ME6217C33M5G (SOT23-5). VIN comes from VBUS (= the 5V pin) and CE is tied to VBUS (always on). Cin is 1uF and Cout 22uF. Datasheet: 800 mA max (VIN >= VOUT+1V, package-limited), dropout 100 mV typ / 180 mV max at 300 mA (3.0-5.5 V outputs), quiescent current ISS1 100 uA typ / 130 uA max (no load), VIN 2-6.5 V (abs max 7 V), thermal shutdown 160 C. | datasheet/medium | nologo schematic POWER block; Microne ME6217 datasheet (LCSC C81592 PDF https://atta.szlcsc.com/upload/public/pdf/source/20170919/C81592_1505787400735977273.pdf) |
| SM-12 | SINGLE-SOURCE | Battery path: B+ (BAT) -> BAT60B Schottky -> VBUS -> LDO. With no USB, the 5V pin equals VBAT minus the diode drop (VF 0.24 V typ at 10 mA, 0.32 V typ at 100 mA, 0.38 V max at 100 mA). The vendor says B+/B- accept 3.3-6 V. The schematic has no over-discharge/protection IC, so use a protected cell. There is no diode between USB VBUS and the 5V header pin. | datasheet/medium | nologo schematic TYPE-C USB block (PD1 BAT60B BAT->VBUS); Infineon BAT60B datasheet (https://static.chipdip.ru/lib/166/DOC026166192.pdf); nologo page |
| SM-13 | SINGLE-SOURCE | Charger: TP4054 (SOT23-5) powered from VBUS. R_PROG = 10 kOhm, giving I_BAT = 1000*V_PROG/R_PROG = 100 mA (93-107 mA). Float voltage is 4.2 V +/-1%. CHRG is open-drain, low while charging, high-Z when done or in UVLO; with no battery the LED flashes quickly. Drain into the BAT pin with no charger supply is 6 uA typ / 10 uA max. BOOST jumper: the vendor says bridging it raises charge current from 100 to 300 mA ('only for batteries > 500 mAh'). The vendor schematic, however, shows PR4 1.5k + PR5 0R(NC), which by the formula would give about 770 mA (above the TP4054 600 mA limit). The values conflict, so do not bridge it. | datasheet/medium | nologo schematic CHARGE block; vendor pinout image text; TP4054(AT) datasheet (https://atta.szlcsc.com/upload/public/pdf/source/20260506/DDB5EDEEF3F4C1E9ABD6DD6FB6789AF6.pdf) |
| SM-14 | SINGLE-SOURCE | VBUS sense on GPIO3: the vendor schematic has PR1 100k (VBUS to USB_IN) and PR2 100k (USB_IN to GND), and its net-alias table ties GP3 to USB_IN. If these parts are fitted, GPIO3 (ADC1_CH2) reads about VBUS/2: about 2.5 V on USB, or about (VBAT-0.24..0.32 V)/2 on battery. This is a ~20 uA permanent drain. It has NOT been verified that a real board has PR1/PR2 fitted. | secondary/low | nologo schematic TYPE-C USB block and right-hand net alias table |
| SM-15 | SINGLE-SOURCE | ADC mapping: ADC1_CH0..CH9 = GPIO1..GPIO10; ADC2_CH0..CH9 = GPIO11..GPIO20. ADC2 cannot be used while Wi-Fi is active; in IDF 5.5.5, adc_oneshot_read() can return ESP_ERR_TIMEOUT. The project already uses ADC1 pins 1,4,5,6,7,8,10, which leaves GPIO2 (ADC1_CH1) and GPIO9 (ADC1_CH8) as clean ADC1 pins for a battery divider. GPIO3 is also ADC1 but is a strapping pin and possibly carries the VBUS/2 divider. | source-or-header/high | IDF v5.5.5 soc/esp32s3/include/soc/adc_channel.h; datasheet v2.2 Section 4.2.2.1; esp_adc/include/esp_adc/adc_oneshot.h |
| SM-16 | SINGLE-SOURCE | ADC accuracy after calibration (12-bit, measured with an external 100 nF capacitor): ATTEN0 0-850 mV +/-5 mV; ATTEN1 0-1100 mV +/-6; ATTEN2 0-1600 mV +/-10; ATTEN3 0-2900 mV +/-50. The Arduino default attenuation is ADC_11db (the deprecated alias of DB_12). analogReadMilliVolts() uses the curve-fitting scheme on S3 (ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED=1). A 1:2 divider puts 4.2 V at 2.1 V, which is inside the ATTEN3 range. | datasheet/high | Datasheet v2.2 Tables 5-5/5-6; cores/esp32/esp32-hal-adc.c line 62; esp_adc/esp32s3/include/adc_cali_schemes.h |
| SM-17 | SINGLE-SOURCE | Chip-level low-power currents at 3.3 V: light sleep 240 uA (+40 uA for 2 MB 4-line PSRAM); deep sleep with RTC memory and RTC peripherals on 8 uA, with RTC peripherals off 7 uA; power off (CHIP_PU low) 1 uA. The vendor claims about 43 uA for the board in deep sleep. That figure is NOT consistent with the board's LDO (100 uA typ Iq) plus the possible VBUS divider (~20 uA) plus WS2812 idle, so it is unverified. | datasheet/high | Datasheet v2.2 Table 5-10; nologo page |
| SM-18 | SINGLE-SOURCE | Recommended operating voltage for VDDA/VDD3P3/VDD3P3_RTC/VDD3P3_CPU is 3.0-3.6 V (typ 3.3). The internal weak pull-up and pull-down are 45 kOhm typ. VIH >= 0.75*VDD, VIL <= 0.25*VDD. | datasheet/high | Datasheet v2.2 Section 5 (recommended operating conditions, DC characteristics Table 5-4) |
| SM-19 | SINGLE-SOURCE | Other GPIO cautions: GPIO39-42 = JTAG (MTCK/MTDO/MTDI/MTMS), GPIO43/44 = UART0 (U0TXD has a 499R series resistor on the board), GPIO19/20 = USB. GPIO46 IS output-capable on S3: SOC_GPIO_VALID_OUTPUT_GPIO_MASK excludes only 22-25. Secondary pinout sites (espboards.dev) that call GPIO9-14 'strapping' and GPIO46 'input only' are wrong for the S3. | source-or-header/high | Datasheet v2.2 Section 2.3.5; IDF v5.5.5 soc/esp32s3/include/soc/soc_caps.h lines 187-189; nologo schematic UR26 499R |
| SM-20 | SINGLE-SOURCE | Arduino core 3.3.12 bundles ESP-IDF v5.5.5 (lib-builder), and esptool 5.3.1 is installed. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/tools/esp32s3-libs/3.3.12/versions.txt; include/esp_common/include/esp_idf_version.h |
| SM-21 | SINGLE-SOURCE | Board settings (boards.txt, 'ESP32S3 Dev Module' = esp32:esp32:esp32s3). Arduino uses the FIRST menu entry as the default. Defaults: USB Mode = 'Hardware CDC and JTAG' (usb_mode=1), USB CDC On Boot = Disabled (must change to Enabled), Upload Mode = 'UART0 / Hardware CDC', Flash Mode = 'QIO 80MHz', Flash Size = 4MB, Partition = 'Default 4MB with spiffs' (app0/app1 0x140000 = 1,310,720 B each, spiffs 0x160000, coredump 64 KB), PSRAM = Disabled, CPU = 240 MHz. For the FH4R2: PSRAM must be 'Disabled' or 'QSPI PSRAM', never 'OPI PSRAM' (octal build). Do not choose 'QIO 120MHz' (in-package flash is 80 MHz). The explicit FQBN esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,PSRAM=disabled,FlashMode=qio,FlashSize=4M,PartitionScheme=default,CPUFreq=240 was accepted by arduino-cli board details; every value except CDCOnBoot is the default. | source-or-header/high | /mnt/wd/arduino-esp32/data/packages/esp32/hardware/esp32/3.3.12/boards.txt lines 1175-1374; tools/partitions/default.csv; arduino-cli board details run |
| SM-22 | SINGLE-SOURCE | Serial mapping: with ARDUINO_USB_CDC_ON_BOOT=1 and ARDUINO_USB_MODE=1, Serial is HWCDCSerial (the USB-C port). With CDC On Boot disabled, Serial is Serial0 = UART0 on GPIO43/44. HWCDC: the default tx_timeout_ms is 100. When the host port is open but not being read, write() retries up to 20 x tx_timeout_ms (about 2 s) per call. Serial.setTxTimeoutMs(0) avoids this. operator bool() is true only when the CDC is connected (never true on battery). HWCDC::isPlugged() uses usb_serial_jtag_is_connected() to detect a USB host. | source-or-header/high | cores/esp32/HardwareSerial.h lines 438-452; cores/esp32/HWCDC.h; cores/esp32/HWCDC.cpp lines 105, 375, 582 |
| SM-23 | SINGLE-SOURCE | Runtime self-check APIs that compile on core 3.3.12: esp_chip_info() features CHIP_FEATURE_EMB_FLASH (BIT0) and CHIP_FEATURE_EMB_PSRAM (BIT7); esp_flash_get_physical_size(NULL,&sz); eFuse fields ESP_EFUSE_FLASH_CAP and ESP_EFUSE_PSRAM_CAP read via esp_efuse_read_field_blob. In esptool's decoding, FLASH_CAP 2 = 'Embedded Flash 4MB', 1 = 8MB, and PSRAM_CAP 2 = 'Embedded PSRAM 2MB', 1 = 8MB. An FH4R2 should read flash_cap=2, psram_cap=2 (not checked on hardware). | compile-test/high | IDF v5.5.5 esp_hw_support/include/esp_chip_info.h; efuse/esp32s3/include/esp_efuse_table.h; esptool targets/esp32s3.py (https://raw.githubusercontent.com/espressif/esptool/master/esptool/targets/esp32 |
| SM-24 | SINGLE-SOURCE | RTC-capable GPIOs on S3 are GPIO0-21 (SOC_RTCIO_PIN_COUNT 22), so the buttons 1/4/5/6 and DRDY 12 can be deep-sleep wake sources. esp_sleep_enable_ext1_wakeup supports ESP_EXT1_WAKEUP_ANY_LOW on S3; ALL_LOW is deprecated as an alias. Digital pads such as GPIO48 can only be held in deep sleep with gpio_hold_en() plus gpio_deep_sleep_hold_en() (both available). | source-or-header/high | IDF v5.5.5 soc/esp32s3/include/soc/soc_caps.h line 329; esp_hw_support/include/esp_sleep.h lines 37-40; esp_driver_gpio/include/driver/gpio.h lines 403-450 |
| SM-25 | SINGLE-SOURCE | Power-up glitches: GPIO1-18 show a ~60 us low-level glitch at chip power-up. GPIO19/20 show two high-level glitches (~60 us each). This does not matter for button inputs, but any future output on these pins (for example a sensor power switch) sees it. | datasheet/high | Datasheet v2.2 Table 2-2 |
| SM-26 | SINGLE-SOURCE | Board size is 22.52 x 18 mm. The antenna is an on-board ceramic chip antenna (the red part marked 'C3', ANT-C3 in the schematic). | secondary/medium | nologo page; nologo schematic (A1 ANT-C3) |

**Pitfalls**

- USB CDC On Boot defaults to Disabled in 'ESP32S3 Dev Module'. Without it, Serial goes to UART0 (GPIO43/44) and the USB-C port shows nothing. The FQBN must include CDCOnBoot=cdc. Add a compile-time #warning when !ARDUINO_USB_CDC_ON_BOOT.
- Never select PSRAM = 'OPI PSRAM' for the FH4R2 (it has quad PSRAM): the build links the octal lib (CONFIG_SPIRAM_MODE_OCT). Guard with #if defined(BOARD_HAS_PSRAM) && defined(CONFIG_SPIRAM_MODE_OCT) #error. Also avoid 'QIO 120MHz' because the in-package flash is 80 MHz.
- Default partition: the app is limited to 1,310,720 B (1.25 MB, two OTA slots). The current self-check sketch uses 310,614 B. If a Wi-Fi time-sync build grows past 1.25 MB, switch to 'No OTA (2MB APP/2MB SPIFFS)' or 'Huge APP (3MB No OTA/1MB SPIFFS)'.
- GPIO48 is shared by the red LED and the WS2812 DIN. digitalWrite(48,HIGH) lights the red LED (~1.5 mA) and drives the WS2812 data line, and rgbLedWrite() sends RMT pulses that make the red LED flicker. Pick one use. Simplest: call rgbLedWrite(48,0,0,0) once at boot and leave the pin LOW. Do not use LED_BUILTIN with digitalWrite expecting a plain LED: on this variant it is a virtual RGB pin (49+48).
- Do not use `while(!Serial)` with HWCDC: it is true only when a host has the CDC port open, so on battery the device hangs forever. Call Serial.setTxTimeoutMs(0) so logging never stalls the UI when the host stops reading.
- Deep sleep makes the COM port disappear, and light sleep can make the host drop it. Expect re-enumeration, and for flashing a sleeping device use BOOT+RESET. The firmware must not touch GPIO19/20.
- GPIO45 must not be pulled high at reset: it would switch VDD_SPI to 1.8 V and the 3.3 V in-package flash would fail to boot. GPIO0 must not be held low at reset by external wiring, or the board enters download mode. Do not put buttons or pull-ups on 0/45/46. GPIO3 is a strapping pin and may carry the VBUS/2 divider, so do not use it for a button.
- ADC2 (GPIO11-20) does not work while Wi-Fi is active. A battery divider must go on ADC1. With the project's pin map the free ADC1 pins are GPIO2 and GPIO9 (GPIO3 is reserved, see above).
- The 5V header pin is VBUS directly, with no diode from USB. On battery it is only VBAT-0.25..0.35 V. Power the OLED and sensors from 3V3, not 5V, and never feed external 5V into the 5V pin while USB is plugged in.
- On battery the 3V3 rail falls out of regulation once VBAT < about 3.3 V + LDO dropout + BAT60B VF (roughly 3.55-3.8 V, depending on load). The chip needs >= 3.0 V on the rail. The battery gauge 'empty' threshold must account for this. The exact cutoff has not been measured.
- No over-discharge protection exists on the board (none in the schematic). Use a protected Li-ion/LiPo cell. Do not bridge the BOOST jumper: the vendor's 300 mA claim and the schematic resistor values (about 770 mA by the formula) disagree.
- Board-level sleep current is dominated by board parts, not the chip. LDO Iq is 100 uA typ, the VBUS divider about 20 uA, TP4054 BAT drain up to 6 uA, and WS2812 idle current is unknown (community measurements 0.4-1 mA for classic WS2812B). The vendor's '43 uA deep sleep' cannot be trusted. Measure it.
- Several secondary pinout sites are wrong for the S3: they call GPIO9-14 or 39-41 'strapping' and GPIO46 'input only'. Trust the Espressif datasheet: strapping pins are 0, 3, 45 and 46 only, and GPIO46 is output-capable.
- Vendor clones differ (factories vary in LED wiring, the divider and the charger). Every board-level fact here comes from the nologo schematic. The user's AITEWIN board is only a visual match.

**Unverified (researcher)**

- That the user's AITEWIN board has the same BOM as the nologo schematic (ESP32-S3FH4R2, ME6217C33, TP4054, BAT60B, red LED + WS2812 on GPIO48). This is a visual PCB match only. Cheapest check: run the self-check sketch and look for flash_cap=2, psram_cap=2, emb_flash=1, emb_psram=1. Then rgbLedWrite(48,0,0,32) should give blue on the WS2812, and digitalWrite(48,HIGH) should light a red LED.
- Whether the GPIO3 VBUS/2 divider (PR1/PR2 100k) is fitted. Check: with USB plugged, analogReadMilliVolts(3) should read about 2500 mV; if it floats or reads ~0, the divider is absent.
- Which LED the vendor text calls 'blue on GPIO48'. The schematic and pinout image say red LED on GPIO48 and blue LED = charge indicator. Not checked on hardware.
- The BOOST jumper charge current (vendor: 300 mA; schematic values imply about 770 mA). Not measured; do not bridge.
- Real board deep-sleep current on battery (vendor claims ~43 uA; the LDO datasheet alone gives 100 uA typ). WS2812 idle current for this part is unknown (0.4-1 mA in community measurements for classic WS2812B; some newer revisions claim <1 uA). Not measured.
- Whether GPIO48 needs gpio_hold during deep sleep to stop the WS2812 or red LED glowing from a floating DIN. The hold snippet compiles, but its effect has not been tested.
- The battery-voltage cutoff at which the 3V3 rail falls below 3.0 V (estimated VBAT about 3.3-3.4 V at light load from dropout + diode VF). The IDF brownout level-7 voltage for S3 was not found in the v5.5 docs.
- The GPIO33 'PWR_PS' net alias in the vendor schematic: no connected part is visible. Assumed to be a plain back pad.
- The ESP32-S3 boot behaviour for GPIO0=0 with GPIO46=1 (an invalid combination per the TRM) was not looked up. It is irrelevant here because GPIO46 is an unconnected back pad with an internal pull-down.
- The vendor's 'press RESET after upload' note: the exact reason (esptool hard_reset after manual download mode over USB-Serial-JTAG) was not verified.

**Recommended snippets**

_Board pin map + boot-time self-check (chip/flash/PSRAM eFuse, USB host, VBUS/2 on GPIO3, BOOT button) with build-setting guards_ (compiled: yes)

```cpp
#include <Arduino.h>
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "esp_system.h"

#if !ARDUINO_USB_CDC_ON_BOOT
#warning "USB CDC On Boot is Disabled: Serial goes to UART0 (GPIO43/44), not the USB-C port"
#endif
#if defined(BOARD_HAS_PSRAM) && defined(CONFIG_SPIRAM_MODE_OCT)
#error "ESP32-S3FH4R2 has QUAD PSRAM: select PSRAM = 'QSPI PSRAM' or 'Disabled', never 'OPI PSRAM'"
#endif

namespace board {
constexpr uint8_t PIN_LED48   = 48;  // red LED (1k to GND) AND WS2812 DIN share GPIO48
constexpr uint8_t PIN_BOOT    = 0;   // BOOT button to GND (strapping pin)
constexpr uint8_t PIN_VBUS_DIV = 3;  // nologo schematic: VBUS-100k-GPIO3-100k-GND ("USB_IN"), unverified on this unit
constexpr int8_t  PIN_BATT_ADC = -1; // future battery divider: use an ADC1 pin (GPIO2 or GPIO9 are free)
}

static void printChipReport() {
  esp_chip_info_t ci;
  esp_chip_info(&ci);
  uint32_t flash_phys = 0;
  esp_flash_get_physical_size(NULL, &flash_phys);
  uint8_t flash_cap = 0, psram_cap = 0;
  esp_efuse_read_field_blob(ESP_EFUSE_FLASH_CAP, &flash_cap, 3);   // FH4R2 expected: 2 (4MB)
  esp_efuse_read_field_blob(ESP_EFUSE_PSRAM_CAP, &psram_cap, 2);   // FH4R2 expected: 2 (2MB)
  Serial.printf("chip=%s rev=v%u.%u cores=%u\n", ESP.getChipModel(),
                ci.revision / 100, ci.revision % 100, ci.cores);
  Serial.printf("emb_flash=%d emb_psram=%d flash_cap=%u psram_cap=%u flash_phys=%lu\n",
                (ci.features & CHIP_FEATURE_EMB_FLASH) ? 1 : 0,
                (ci.features & CHIP_FEATURE_EMB_PSRAM) ? 1 : 0,
                flash_cap, psram_cap, (unsigned long)flash_phys);
  Serial.printf("psram_heap=%lu reset_reason=%d\n", (unsigned long)ESP.getPsramSize(),
                (int)esp_reset_reason());
}

void setup() {
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);  // never block UI when the host is not reading
#endif
  Serial.begin(115200);
  rgbLedWrite(board::PIN_LED48, 0, 0, 0);  // WS2812 off; line idles LOW -> red LED off
  analogSetPinAttenuation(board::PIN_VBUS_DIV, ADC_11db);
  printChipReport();
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    uint32_t mv = analogReadMilliVolts(board::PIN_VBUS_DIV);
#if ARDUINO_USB_MODE && ARDUINO_USB_CDC_ON_BOOT
    bool host = HWCDC::isPlugged();
#else
    bool host = false;
#endif
    Serial.printf("gpio3=%lumV (vbus~%lumV) usb_host=%d boot_btn=%d\n",
                  (unsigned long)mv, (unsigned long)(mv * 2), host ? 1 : 0,
                  digitalRead(board::PIN_BOOT) == LOW ? 1 : 0);
  }
}
```

_Future battery ADC (one config line) + GPIO48 LED-off/hold around deep sleep_ (compiled: yes)

```cpp
#include "driver/gpio.h"

namespace battery {
// Future divider: B+ --R1--+--R2-- GND, midpoint to an ADC1 pin (GPIO2 or GPIO9). Add 100 nF at the ADC pin.
// ratio = (R1 + R2) / R2. pin < 0 means "no battery hardware" -> UI shows "--".
struct Config { int8_t pin; float ratio; };
constexpr Config CFG = { -1, 2.0f };

inline bool present() { return CFG.pin >= 0; }

inline int32_t readMv() {
  if (!present()) return -1;
  analogSetPinAttenuation(CFG.pin, ADC_11db);   // ~0..2.9 V usable range on S3 (+/-50 mV calibrated)
  uint32_t acc = 0;
  for (int i = 0; i < 16; ++i) acc += analogReadMilliVolts(CFG.pin);
  return (int32_t)((float)(acc / 16) * CFG.ratio);
}
}  // namespace battery

namespace led48 {
inline void releaseAfterWake() {          // call early in setup(): hold survives deep-sleep reset
  gpio_hold_dis(GPIO_NUM_48);
  gpio_deep_sleep_hold_dis();
}
inline void offAndHoldForDeepSleep() {    // call right before esp_deep_sleep_start()
  rgbLedWrite(48, 0, 0, 0);               // WS2812 dark
  pinMode(48, OUTPUT);
  digitalWrite(48, LOW);                  // red LED off, WS2812 DIN not floating
  gpio_hold_en(GPIO_NUM_48);
  gpio_deep_sleep_hold_en();
}
}  // namespace led48
```

_Build settings: arduino-cli FQBN (validated with 'board details'; all values except CDCOnBoot are defaults) and the matching Arduino IDE menu_ (compiled: no)

```cpp
# arduino-cli (explicit, equivalent to esp32:esp32:esp32s3:CDCOnBoot=cdc)
esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,UploadMode=default,PSRAM=disabled,FlashMode=qio,FlashSize=4M,PartitionScheme=default,CPUFreq=240

# Arduino IDE: Tools ->
#   Board: ESP32S3 Dev Module
#   USB CDC On Boot: Enabled            <- the only non-default
#   USB Mode: Hardware CDC and JTAG     (default)
#   Upload Mode: UART0 / Hardware CDC   (default)
#   Flash Mode: QIO 80MHz               (default; NOT 'QIO 120MHz')
#   Flash Size: 4MB (32Mb)              (default)
#   Partition Scheme: Default 4MB with spiffs (1.2MB APP/1.5MB SPIFFS)  (or 'No OTA (2MB APP/2MB SPIFFS)' if app > 1.25 MB)
#   PSRAM: Disabled (default) or 'QSPI PSRAM' -- NEVER 'OPI PSRAM'
#   CPU Frequency: 240MHz (WiFi)
# Manual download mode: hold BOOT, tap RESET, release BOOT (or hold BOOT while plugging USB); press RESET after upload.
```

