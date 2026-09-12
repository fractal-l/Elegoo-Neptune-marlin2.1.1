# Flashing — Neptune 3 Pro (MKS E3D V2, no USB serial)

## 1. Mainboard firmware

File: `firmware/ZNP_ROBIN_NANO-UBL121.bin` (208 KB, md5 `25038cb9e7558a317ca2d5bc4a938d44`)

1. Copy it to a FAT32 micro-SD card as **`ZNP_ROBIN_NANO.bin`**
   (the bootloader only accepts that exact name).
2. Power off the printer, insert the SD into the **mainboard** slot,
   power on, wait ~30 s until it boots to the logo screen.
3. `M503` equivalent: verify version on screen shows UBL build,
   then run bed leveling once: screen → Level → auto-level
   (runs `G29 P1 / P3.2 / S1 / F10.0 / A` + `M500`).

## 2. Touchscreen firmware (stock Elegoo 1.4.2)

File: `screen/TFT/Elegoo-N3Pro-stock-1.4.2.tft` (6.2 MB)

Only needed if your screen is blank/corrupt or was flashed with a
custom theme (e.g. FractalUI remnants). The UBL firmware works with
the stock screen as-is — mesh display decimates 121→36 points.

1. Copy the `.tft` to a FAT32 micro-SD card (screen slot, **not** mainboard).
2. Power on — the screen flashes itself, then shows the UI.

## 3. No-USB tuning (all via SD `.gcode` files)

- `M593 F35` then `M500` — input-shaping frequency after a ringing tower
- `G26` — UBL mesh validation pattern
- `M303 E0 S210 C5` then `M500` — PID autotune (heaters auto-shutoff fixed)
