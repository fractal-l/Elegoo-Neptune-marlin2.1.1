# FractalUI v1 — firmware-rendered overlay for the Elegoo TJC screen

## Concept

The stock screen firmware (Elegoo **V1.4.2**, `3D38_20230520.tft`) is flashed **unchanged**.
It keeps its 83 pages, touch hotspots and the `5A A5` key protocol. The Marlin driver
takes over rendering: on every page entry it fills the screen with a dark background and
draws the entire UI over serial (`xstr`, `fill`, `line`). No Elegoo art is visible in
normal use (a ~100 ms flash on page switch; eliminated later by Option B picture patching).

- Coordinate space: **272x480 portrait** (displayed rotated 90° = physical 480x272).
- Fonts: **0** (ASCII, ~30px) only in v1.
- All UI logic, strings and state live in Marlin (`fractalui.cpp`).
- Touch: stock hotspots emit the stock `5A A5` key frames; existing fork handlers keep
  their action logic; UI-push lines are rerouted to FractalUI.

## Page sync

`sendme` is enabled at boot → the screen emits `0x66 <idx> FF FF FF` on every page change.
The driver learns the index↔name table at boot by probing each page by name
(`page <name>`) and recording the `0x66` reply. All switches go through:

- `FUI::goto_name(name)` — `page <name>` + draw.
- `FUI::page_index(N)` — stock `ExchangePageBase+N` VP write; draw happens on the
  `0x66` confirmation via the learned table (wrong guesses impossible).
- Unsolicited `0x66` (screen-side navigation) → learned table → draw.

## Palette (RGB565)

| Token    | Hex web  | Use                     |
|----------|----------|-------------------------|
| BG       | #0D1117  | page background         |
| CARD     | #161B22  | cards, buttons          |
| CARD2    | #21262D  | pressed/alt buttons     |
| BORDER   | #30363D  | separators, outlines    |
| TEXT     | #E6EDF3  | primary text            |
| MUTED    | #8B949E  | labels, secondary       |
| ACCENT   | #58A6FF  | values, active elements |
| RED      | #F85149  | errors, hot             |
| GREEN    | #3FB950  | ok, heating done        |
| AMBER    | #D29922  | warnings                |

## Screens (stock page → FractalUI draw)

- `main` — dashboard: state + nozzle/bed/fan header, 2x2 menu (Print/Prepare/Settings/Level)
- `printfiles`, `file1..file5` — file browser (rows over stock row hotspots)
- `askprint` — selected file + Print/Back
- `printpause` — print dashboard: file, progress bar, elapsed/left, temps, speed/flow, XYZ; Pause/Stop/LED buttons
- `pauseconfirm`, `resumeconfirm`, `cancleheat` — confirm dialogs
- `nofilament`, `noFilamentPush`, `filamentresume`, `heatfilament` — M600 / filament flow
- `premove` — jog pad: axis select, D-pad, Z, step, extrude/retract
- `pretemp` — PLA/PETG/ABS/TPU presets + nozzle/bed target
- `prefilament` — load/unload/stop
- `adjusttemp` — target adjust (±, steps, nozzle/bed)
- `adjustspeed` — live tuning: speed/flow tabs, ±, cycle
- `autohome`, `wait` — progress
- `tempset`, `tempsetvalue` — preset list / edit
- `set` — settings menu (toggles, factory, info)
- `information` — **stats**: total prints, total print time, filament used, versions, maintenance hints (new EEPROM fields)
- `multiset` — advanced menu (PLR toggle, file display toggle)
- `leveling`, `leveling_36/49/63` — ABL progress
- `leveldata_36/49/63`, `aux49_data`, `aux63_data` — level done + Z-offset (heatmap later)
- `warn_rdlevel`, `warn_zoffset`, `warn_aux`, `tips_level`, `level_aux`, `aux2autolevel` — level dialogs
- `err_*` (10) — error screens
- `nosdcard`, `printfinish`, `continueprint`, `factorysetting`, `language(s)` — utility screens
- `boot`, `hardwaretest`, `motortest`, `keybdB` — left stock (service pages)

## Files

- New: `Marlin/src/lcd/extui/dgus/elegoo/fractalui.{h,cpp}`
- Modified: `tjc_page.h` (body → `FUI::goto_name`), `DGUSDisplayDef.cpp`
  (90 `ExchangePageBase` sites → `FUI::page_index`; `EachMomentUpdate` → `FUI::tick`;
  `RTS_RecData` `0x66` branch), `settings.cpp` (stats fields; EEPROM V90 → V91).

## Known v1 limitations

- ~100 ms stock-art flash on page switch (fixed by Option B later).
- Hotspot press-highlight bleeds briefly (covered by redraw on key receipt).
- Only ASCII (font 0). One extra font could be imported with Option B.
