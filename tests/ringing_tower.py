#!/usr/bin/env python3
"""
Ringing-tower generator for input-shaping tuning (Neptune 3 Pro, Marlin M593).

Prints a thin-walled square tube in 4 height zones; M593 F is changed at each
zone boundary so ONE print compares four settings:

  zone 1 (0-6mm):    shaping OFF  (baseline: ringing + ghosting worst)
  zone 2 (6-12mm):   M593 F30
  zone 3 (12-18mm):  M593 F40
  zone 4 (18-24mm):  M593 F50

Read the OUTER walls at the 90-degree corners: ghosting (an echo of the corner,
a wavy line continuing past it) should shrink in the best zone. Zone 1 is the
"before" reference. Continuous single loop per layer => only ONE line start for
the whole print (the purge), so wet-filament blobs at line starts cannot ruin
the read.

Usage: python3 ringing_tower.py > ringing_tower.gcode
Copy to SD, print, pick the cleanest F, then put `M593 F<val> D0.15` + `M500`
in a tuning .gcode on the SD and print it to save.
"""
import sys

# --- machine / material (Neptune 3 Pro + PETG 240/80, 0.4 nozzle) ---
TEMP_NOZZLE = 240
TEMP_BED    = 80
BED_X, BED_Y = 235.0, 232.0                 # bed size
CX, CY = (BED_X - 5.0) / 2.0, BED_Y / 2.0   # bed center (X_MIN_POS = -5)
NOZ   = 0.4
LAYER = 0.2
FIRST_Z = 0.2                             # true first-layer height for z-offset ~-0.2
                                          # (nozzle touches bed at Z~0.2 after G28)
FIL_D = 1.75
E_AREA = 3.14159265 * FIL_D * FIL_D / 4.0

SIDE = 30.0                                 # outer square side
RING_GAP = 0.4                              # wall spacing = line width
LAYERS_PER_ZONE = 30                        # 6 mm per zone
ZONES = [(0.0, "OFF"), (30.0, "F30"), (40.0, "F40"), (50.0, "F50")]

FEED_FIRST  = 30 * 60
FEED_OUTER  = 100 * 60
FEED_TRAVEL = 150 * 60


def e_per_mm(width=NOZ, height=LAYER):
    return width * height / E_AREA


def ring_path(side, z, e_ratio):
    """Closed CCW square perimeter at height z -> gcode lines."""
    half = side / 2.0
    pts = [(CX - half, CY - half), (CX + half, CY - half),
           (CX + half, CY + half), (CX - half, CY + half)]
    out = [f"G1 X{pts[0][0]:.2f} Y{pts[0][1]:.2f} F{FEED_TRAVEL}",
           f"G1 Z{z:.2f} F600"]
    d = 0.0
    for i in range(4):
        x1, y1 = pts[i]
        x2, y2 = pts[(i + 1) % 4]
        d += abs(x2 - x1) + abs(y2 - y1)
        out.append(f"G1 X{x2:.2f} Y{y2:.2f} E{d * e_ratio:.3f} F{FEED_OUTER}")
    return out


def main():
    inner = SIDE - 2 * RING_GAP
    total_layers = LAYERS_PER_ZONE * len(ZONES)

    L = [
        ";ringing_tower - M593 ZV comparison, Neptune 3 Pro",
        ";zones bottom->top: OFF / F30 / F40 / F50, 6mm each, 100mm/s outer",
        ";read outer walls at corners: least ghosting wins -> M593 F<val> D0.15 + M500",
        f"M104 S{TEMP_NOZZLE}",
        f"M140 S{TEMP_BED}",
        f"M190 S{TEMP_BED}",
        f"M109 S{TEMP_NOZZLE}",
        "G28",
        "M420 S1 ; enable saved 121pt UBL mesh (also auto-loaded at boot)",
        "G90",
        "M82",
    ]

    # Purge line, then jump to the tower start without a seam on the tube.
    L += [f"G1 X{CX - 60:.2f} Y{CY - 70:.2f} Z{FIRST_Z:.2f} F{FEED_TRAVEL}",
          "G92 E0",
          f"G1 X{CX - 30:.2f} Y{CY - 70:.2f} E{30 * e_per_mm():.3f} F{FEED_FIRST}",
          "G1 Z5 F600",
          f"G1 X{CX - SIDE / 2:.2f} Y{CY - SIDE / 2:.2f} F{FEED_TRAVEL}",
          "G92 E0"]

    for li in range(total_layers):
        if li % LAYERS_PER_ZONE == 0:
            freq, name = ZONES[min(li // LAYERS_PER_ZONE, len(ZONES) - 1)]
            L.append(f";--- zone {name} ---")
            L.append("M593 F0 ; shaping OFF" if freq == 0 else f"M593 F{freq:.0f}")
        z = FIRST_Z + li * LAYER   # li=0 -> 0.20 (first layer), li=1 -> 0.40, ...
        er = e_per_mm(NOZ, LAYER)
        L += ring_path(SIDE, z, er)
        L += ring_path(inner, z, er)

    # Retract-ish cleanup + cool down
    L += ["M107", "M104 S0", "M140 S0",
          f"G1 Z{FIRST_Z + total_layers * LAYER + 5:.2f} F600",
          f"G1 X{CX + 80:.2f} Y{CY - 70:.2f} F{FEED_TRAVEL}",
          "M84", "END"]
    print("\n".join(L))


if __name__ == "__main__":
    sys.exit(main())
