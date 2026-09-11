# Changelog — Elegoo Neptune 3 (fractal-l fork)

Firmware for the Elegoo Neptune 3 PRO / PLUS / MAX, based on
[NARUTOfzr/Elegoo-Neptune-marlin2.1.1](https://github.com/NARUTOfzr/Elegoo-Neptune-marlin2.1.1),
which is based on **Marlin 2.1.1** (upstream tag `398cae7`) with MKS E3D V2 board
support and an Elegoo TJC touchscreen UI.

## [Unreleased] — `fix/safety-and-cleanup`

Safety fixes and cleanup on top of the Elegoo fork (`be2bc9f`, `1.x.5.1b`):

- **fix(thermal):** re-enabled `disable_all_heaters()` before PID autotune
  (`M303`). The fork had commented it out, allowing autotune to run while the
  other heater was still live (hazard + skewed tune).
- **fix(safety):** a failed homing move no longer auto-recovers during an
  active print job. Previously the firmware did `stop(); delay(5000);
  marlin_state = MF_RUNNING;` — the printer declared itself healthy with no
  known axis origin, so a failed G28 in print start gcode could crash the
  nozzle into the bed. Now: user-initiated homes keep the auto-recover
  behavior; failures while `printJobOngoing() || IS_SD_PRINTING()` stay in
  `MF_STOPPED` until `M999`. The blocking `delay(5000)` became a
  watchdog-safe `safe_delay(5000)` in both sites
  (`Endstops::validate_homing_move()`, `homeaxis()` endstop pre-check).
- **fix(abort):** removed the unconditional `M500` (full EEPROM write) from
  the SD-abort path — it fired on every stopped print (flash wear, plus a
  multi-second stall given the fork's slowed EEPROM writes). `M900 K0` and
  `M84` are kept.
- **refactor(ui):** added `tjc_page()` helper (`lcd/extui/dgus/elegoo/tjc_page.h`)
  and replaced 106 occurrences of the two-line
  `LCD_SERIAL_2.printf("page X"); printf("\xff\xff\xff");` pattern across
  10 files. No behavior change.
- **docs/ci:** this changelog; GitHub Actions workflow building the
  `MKS_E3_V2` environment and uploading `ZNP_ROBIN_NANO.bin`.

## [1.x.5.1b] — 2023-07-03 (Elegoo fork baseline, `be2bc9f`)

Differences vs upstream Marlin 2.1.1 (`398cae7`). 41 files modified, 3 added.

### 1. New hardware support

- **MKS E3D V2 board** (MKS Robin E3D, STM32F401RCT6): new board id `4241`
  (`BOARD_MKS_E3D_V2`), new pin map `pins/stm32f4/pins_MKS_E3_V2.h`.
- **PlatformIO env `MKS_E3_V2`** (`ini/stm32f4.ini`): custom board
  `buildroot/share/PlatformIO/boards/marlin_MKS_F401RC.json`, flash offset
  `0x8000`, output renamed to `ZNP_ROBIN_NANO.bin`, J-Link upload protocol.
- **Ender-3 S1 F4 ELE pin variant**: new `pins_CREALITY_V24S1_301F4_ELE.h`.
- **Second LCD UART**: `LCD_SERIAL_PORT_2` / `LCD_SERIAL_2`
  (`HAL/STM32/HAL.h`) used as a raw TJC/Nextion-style serial channel
  alongside the DGUS RTS protocol.

### 2. Elegoo TJC/DGUS touchscreen UI

- New `lcd/extui/dgus/elegoo/` directory (~8,600 lines:
  `DGUSDisplayDef.{cpp,h}`), gated by `RTS_AVAILABLE` / `TJC_AVAILABLE`
  (defined in `Configuration.h`).
- Full Neptune 3 screen integration: print/pause/stop, temperature control,
  preheat (PLA/ABS/TPU/PETG), file browser with pagination, leveling UI
  with live mesh progress, feed/percent tuning, babystepping, LED case
  light, filament runout dialogs, M600 filament change flow.
- Error pages pushed to the screen: `err_heatfail`, `err_nozzleover`,
  `err_nozzleunde`, `err_bedover`, `err_bedunder`, `err_nozzleheat`,
  `err_bedheat`, `err_homefail`, `err_probefail`, `err_sdread`,
  `err_sdwrite`.
- Custom G-code **`M10088`** (UI-driven pause/resume coordination).
- `M600`/`M300`/`M75`–`M78` wired to screen state (pause screens, beeps,
  job timer icons).

### 3. Core behavioral changes vs stock Marlin

- **Homing failure**: stock `kill(MSG_KILL_HOMING_FAILED)` replaced by
  `stop(); delay(5000); MF_RUNNING` (auto-recover). *(Partially reverted —
  see Unreleased fix above.)*
- **PID autotune**: `disable_all_heaters()` commented out at autotune
  start. *(Reverted — see Unreleased fix above.)*
- **`wait_for_user_response()`** bypassed when `RTS_AVAILABLE` (returns
  immediately; the screen handles user interaction).
- **SD abort path**: raises Z by 5 mm, injects `EVENT_GCODE_SD_ABORT_2`
  when screen-initiated, then enqueues `M900 K0` / `M500` / `M84`
  (steppers off). *(M500 removed — see Unreleased fix above.)*
- **Advanced pause tuning**: retract −3 mm @ 1200 mm/min, prime +2 mm @
  200 mm/min; park position (X−5, Y+0, Z+5).
- **Z fade**: stock linear fade replaced by a quadratic curve in
  `planner.h`:
  `z_fade_factor = 1.003 * (1 - (rz * inverse_z_fade_height)²)`
  (compensates mesh averaging at height; note the 1.003 overshoot factor).
- **EEPROM writes slowed** (`HAL/STM32/eeprom_wired.cpp`):
  `delay(2)→delay(20)` per page and `safe_delay(2)→safe_delay(100)`
  watchdog path.
- Auto-fan sanity check commented out; thermistor table 1 retuned for the
  Neptune hotend/bed; power-loss recovery and G29 mesh progress adapted
  for the screen.

### 4. Firmware configuration (vs stock `Configuration*.h`)

- Per-model conditional blocks: `NEPTUNE_3_PRO` / `NEPTUNE_3_PLUS` /
  `NEPTUNE_3_MAX` (bed size, Z height, mesh points).
- Motion: default acceleration 1000, jerk X/Y 8 / Z 0.4, S-curve smoothing
  enabled, `LIN_ADVANCE` compiled in with `LIN_ADVANCE_K 0.00`.
- Probe: `FIX_MOUNTED_PROBE`, `NOZZLE_TO_PROBE_OFFSET {-28.5, 22, 0}`,
  `Z_SAFE_HOMING`, `Z_PROBE_END_SCRIPT`.
- Leveling: `AUTO_BED_LEVELING_BILINEAR` with mesh extrapolation
  (`BILINEAR_EXTRAPOLATION`... per-model grid) and X/Y skew compensation.
- Enabled: `EEPROM_SETTINGS`, `SDSUPPORT`, power-loss recovery,
  babystepping, `ADVANCED_PAUSE_FEATURE`, `CASE_LIGHT_PIN`, preheat
  presets 1–4 (PLA/ABS/TPU/PETG).
- `BUFSIZE 16` (stock 4) to absorb screen command bursts.

### 5. EEPROM schema

- EEPROM version bumped to **`V86`** with 10 new fields (per-material
  preheat temperatures for the screen, `probe_extrusion_temp`,
  `probe_bed_temp`). **Incompatible with stock Marlin 2.1.1** — settings
  reset to defaults when switching firmware.
