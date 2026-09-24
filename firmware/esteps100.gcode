;esteps100 - extrude exactly 100mm for E-steps calibration
;preheat happens here, one click from SD
M109 S240 ; wait for nozzle temp
G92 E0
G1 E100 F120 ; 100mm slow (2mm/s)
M104 S0 ; cool down
M84
END
