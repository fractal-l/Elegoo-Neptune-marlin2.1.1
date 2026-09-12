/**
 * Elegoo Neptune 3 Pro — TJC/DGUS screen page names (stock screen firmware).
 *
 * First step of the DGUSDisplayDef.cpp split: single source of truth for
 * page names so typos fail at compile time (as constexpr) instead of
 * silently showing the wrong screen at runtime.
 *
 * UBL note: the stock screen only has fixed mesh-view pages
 * (leveldata_36 = 6x6 for the Pro, aux49_data = 7x7, aux63_data).
 * The firmware mesh is 11x11 (121 pts); the display sender decimates
 * step-2 to 36 points for leveldata_36. Full mesh stays in ubl.z_values.
 *
 * Usage: tjc_page(PAGE_MAIN);  (tjc_page takes const char*)
 */
#pragma once

#define PAGE_BOOT            "boot"
#define PAGE_MAIN            "main"
#define PAGE_FILE1           "file1"
#define PAGE_NOSDCARD        "nosdcard"
#define PAGE_ASKPRINT        "askprint"
#define PAGE_PRINTCNFRIM     "printcnfirm"
#define PAGE_CONTINUEPRINT   "continueprint"
#define PAGE_PRINTPAUSE      "printpause"
#define PAGE_PAUSECONFIRM    "pauseconfirm"
#define PAGE_WAIT            "wait"
#define PAGE_AUTOHOME        "autohome"
#define PAGE_PREMOVE         "premove"
#define PAGE_PRETEMP         "pretemp"
#define PAGE_PREFILAMENT     "prefilament"
#define PAGE_HEATFILAMENT    "heatfilament"
#define PAGE_ADJUSTTEMP      "adjusttemp"
#define PAGE_ADJUSTSPEED     "adjustspeed"
#define PAGE_ADJUSTZOFFSET   "adjustzoffset"
#define PAGE_WARN_ZOFFSET    "warn_zoffset"
#define PAGE_TEMPSETVALUE    "tempsetvalue"
#define PAGE_MULTISET        "multiset"
#define PAGE_HARDWARETEST    "hardwaretest"
// Mesh viewers (stock fixed sizes — see UBL note above)
#define PAGE_LEVELDATA_36    "leveldata_36"
#define PAGE_AUX49_DATA      "aux49_data"
#define PAGE_AUX63_DATA      "aux63_data"
// Filament / error pages
#define PAGE_NOFILAMENT      "nofilament"
#define PAGE_NOFILAMENTPUSH  "noFilamentPush"
#define PAGE_FILAMENTRESUME  "filamentresume"
#define PAGE_WARN1_FILAMENT  "warn1_filament"
#define PAGE_WARN2_FILAMENT  "warn2_filament"
#define PAGE_ERR_NOZZLEUNDE  "err_nozzleunde"
#define PAGE_ERR_BEDUNDER    "err_bedunder"
