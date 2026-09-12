# Elegoo TJC screen — page map + UBL notes (Neptune 3 Pro)

Stock screen firmware exposes these pages (see `elegoo_pages.h`).
Firmware sends `page <name>` + `FF FF FF` via `tjc_page()`.

## Mesh viewing with UBL 11x11

| Page | Size | Used for |
|---|---|---|
| `leveldata_36` | 6x6 = 36 | **Pro** display (decimated step-2 from 121-pt mesh) |
| `aux49_data` | 7x7 = 49 | Plus (unused on Pro-only builds) |
| `aux63_data` | ~63 | Max (unused on Pro-only builds) |

Full 121-pt mesh lives in `bedlevel.z_values` (type `unified_bed_leveling` under UBL) + EEPROM slot 1.
Screen button flow: `G29 P1` (probe) → `G29 P3.2` (smart fill) →
`G29 S1` (save) → `G29 F10.0` (fade) → `G29 A` (activate) → `M500`.

Validate with `G26` (mesh validation pattern) and `M420 V` (dump mesh).

## No-USB tuning (SD-card G-code files)

No USB serial on stock board — put commands in a `.gcode` file and "print" it:

- `M593 F35` — set shaping freq, then `M500` to save
- `M48 P10` — probe repeatability test
- `M420 V` response is not visible without host; verify by print quality instead

## Further split plan (for contributors)

`DGUSDisplayDef.cpp` (~8k lines) should become `elegoo/pages/<name>.cpp`,
one TU per screen group (print / temp / move / level / filament),
sharing `elegoo_pages.h` + a `tjc_send()` transport. Do it one page
group per PR so `MKS_E3_V2` keeps building.
