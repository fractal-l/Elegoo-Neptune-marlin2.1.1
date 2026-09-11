/**
 * FractalUI — firmware-rendered dark UI overlay for the Elegoo TJC screen.
 * See docs/fractalui-spec.md for the design.
 *
 * The stock screen firmware (Elegoo V1.4.2) runs unchanged. This module:
 *  - learns the screen's page indices at boot (`sendme` + probe by name),
 *  - draws a dark, information-dense UI over every page (xstr/fill/line),
 *  - refreshes live values ~4 Hz with dirty-region redraws,
 *  - tracks usage statistics persisted in EEPROM.
 */
#include "../../../../inc/MarlinConfig.h"

#if ENABLED(RTS_AVAILABLE)

#include "fractalui.h"

#include "../../../../module/temperature.h"
#include "../../../../module/planner.h"
#include "../../../../module/printcounter.h"
#include "../../../../module/stepper.h"
#include "../../../../module/motion.h"
#include "../../../../module/settings.h"
#include "../../../../gcode/gcode.h"
#include "../../../../sd/cardreader.h"
#include "../../../../lcd/extui/ui_api.h"
#include "../../../../module/probe.h"
#include "DGUSDisplayDef.h"

#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Page registry
// ---------------------------------------------------------------------------

enum FuiPage : uint8_t {
  PG_NONE = 0,
  PG_MAIN, PG_PRINTFILES, PG_FILE1, PG_FILE2, PG_FILE3, PG_FILE4, PG_FILE5,
  PG_ASKPRINT, PG_PRINTPAUSE, PG_PAUSECONFIRM, PG_RESUMECONFIRM,
  PG_CANCLEHEAT, PG_NOFILAMENT, PG_NOFILAMENTPUSH, PG_FILAMENTRESUME,
  PG_HEATFILAMENT, PG_PREMOVE, PG_PRETEMP, PG_PREFILAMENT,
  PG_ADJUSTTEMP, PG_ADJUSTSPEED, PG_AUTOHOME, PG_WAIT,
  PG_TEMPSET, PG_TEMPSETVALUE, PG_SET, PG_INFORMATION, PG_MULTISET,
  PG_LEVELING, PG_LEVELDATA, PG_WARNRDLEVEL, PG_NOSDCARD, PG_PRINTFISH,
  PG_CONTINUEPRINT, PG_FACTORYSETTING, PG_LANGUAGE,
  PG_ERR_NOZZLEHEAT, PG_ERR_BEDHEAT, PG_ERR_HEATFAIL,
  PG_ERR_NOZZLEOVER, PG_ERR_NOZZLEUNDE, PG_ERR_BEDOVER, PG_ERR_BEDUNDER,
  PG_ERR_HOMEFAIL, PG_ERR_PROBEFAIL, PG_ERR_SDREAD, PG_ERR_SDWRITE,
  PG_COUNT
};

static const char *const fui_page_names[PG_COUNT] = {
  nullptr,
  "main", "printfiles", "file1", "file2", "file3", "file4", "file5",
  "askprint", "printpause", "pauseconfirm", "resumeconfirm",
  "cancleheat", "nofilament", "noFilamentPush", "filamentresume",
  "heatfilament", "premove", "pretemp", "prefilament",
  "adjusttemp", "adjustspeed", "autohome", "wait",
  "tempset", "tempsetvalue", "set", "information", "multiset",
  "leveling_36", "leveldata_36", "warn_rdlevel", "nosdcard", "printfinish",
  "continueprint", "factorysetting", "language",
  "err_nozzleheat", "err_bedheat", "err_heatfail",
  "err_nozzleover", "err_nozzleunde", "err_bedover", "err_bedunder",
  "err_homefail", "err_probefail", "err_sdread", "err_sdwrite"
};

// learned index -> page (filled at boot by probing)
static uint8_t fui_idx_to_page[128];
static uint8_t fui_page_to_idx[PG_COUNT];

// ---------------------------------------------------------------------------
// Stock visual components to hide per page (they would repaint over the
// overlay when the fork pushes VP values). Touchable components are kept.
// Generated from the stock HMI exports.
// ---------------------------------------------------------------------------
static const char *const fui_hide_list[] = {
  "adjustspeed	targetspeed t5 t6 t7 t1 t2 t3 t8 p0 p1 p2",
  "adjusttemp	targettemp nozzletemp bedtemp t2 t1 t3 t4 t7 t5 t6 t8 p0 p1",
  "adjustzoffset	t5 t6 t7 t2 t8 t9 t1 t10 p0",
  "askprint	p0",
  "autohome	nozzletemp bedtemp t1 p0",
  "aux2autolevel	p0",
  "aux49_data	level_nozzel level_bed t0 t1 t3 p0",
  "aux63_data	level_nozzel level_bed t0 t1 t3 p0",
  "boot	t0 logo Bar",
  "cancleheat	p0",
  "continueprint	t1 t0 t4 p0",
  "err_bedheat	t0 t1 p0",
  "err_bedover	t0 t1 p0",
  "err_bedunder	t0 t2 p0 p1",
  "err_heatfail	t0 t1 p0",
  "err_homefail	t1 p0",
  "err_nozzleheat	t1 t0 p0",
  "err_nozzleover	t0 t1 p0",
  "err_nozzleunde	t0 t2 p0 p1",
  "err_probefail	t0 p0 p1",
  "err_sd	t1 p0 p1",
  "err_sdread	t1 p0 p1",
  "err_sdwrite	t1 p0 p1",
  "factorysetting	t1 p0",
  "filamentcheck	nozzletemp t2 t3 p0",
  "filamentresume	t2 t3 nozzletemp t1 t5 t6 p0",
  "file1	t25",
  "file2	t25",
  "file3	t25",
  "file4	t25",
  "file5	t25",
  "hardwaretest	nozzletemp bedtemp led2 p1 fanstatue",
  "heatfilament	nozzletemp p0",
  "information	t0 t1 t2 t3 t4 t5 machine size sversion lversion t10 t6 t7",
  "keybdB	t0 show p0",
  "language	t0",
  "languageset	t0 p0 p1 p2 p3 p4 p5 p6 p7",
  "ledcontrl	t2 t3",
  "level_aux	t1",
  "leveldata	nozzletemp bedtemp level_nozzel level_bed t3 p0 p1",
  "leveldata_36	nozzletemp bedtemp level_nozzel level_bed t3 p0 p1",
  "leveldata_49	nozzletemp bedtemp level_nozzel level_bed t3 p0 p1",
  "leveling	level_heating nozzletemp bedtemp",
  "leveling_36	level_heating nozzletemp bedtemp",
  "leveling_49	bedtemp nozzletemp level_heating",
  "leveling_63	bedtemp nozzletemp level_heating",
  "main	nozzletemp bedtemp t0 t1 t2 t3 p2 p3 p4 p1",
  "motorsetting	t0 t5",
  "motorsetvalue	t0 p1",
  "multiset	t0 t2 t7 t1 t5 t6 t8 p0",
  "noFilamentPush	p0",
  "nofilament	t1 p0 p1",
  "nosdcard	t1 p0",
  "pauseconfirm	t1 p0",
  "prefilament	nozzletemp bedtemp t4 t3 t5 t0 t1 t2",
  "premove	t2 t1 t0 t3 p0",
  "pretemp	t2 t1 t0 nozzletemp bedtemp t3",
  "printcnfirm	p0",
  "printfiles	t25 p0 p1 p2 p3 p4 p5 p6 p7",
  "printfinish	t3 t2 t0 t1 t4",
  "printpause	t0 printtime nozzletemp bedtemp printspeed fanspeed t4 t5 printvalue t1 t2 t3 t6 Bar",
  "resumeconfirm	t1 p0",
  "set	t0 t1 t2 t3 t4 t5 t6 t7 t8 t9",
  "speedsetting	t6 t0 t1",
  "speedsetvalue	xaxis yaxis zaxis eaxis t0 t1 t2 t4 t3 t5 t6 t7 p1",
  "tempset	t5",
  "tempsetvalue	t5 p1",
  "tips_level	p0",
  "wait	t1",
  "warn1_filament	p0",
  "warn2_filament	p0",
  "warn_aux	p0",
  "warn_rdlevel	p0",
  "warn_zoffset	t0 t2 t7 p0 p1",
  nullptr
};;

static void fui_send_hides(const char *page_name) {
  if (!page_name) return;
  for (uint8_t i = 0; fui_hide_list[i]; i++) {
    const char *e = fui_hide_list[i];
    const char *tab = strchr(e, '\t');
    if (!tab) continue;
    if ((size_t)(tab - e) != strlen(page_name) || strncmp(e, page_name, tab - e)) continue;
    // matched: hide each component
    const char *p = tab + 1;
    while (*p) {
      const char *q = p;
      while (*q && *q != ' ') q++;
      if (q > p) {
        LCD_SERIAL_2.printf("vis %.*s,0\xff\xff\xff", (int)(q - p), p);
      }
      p = (*q == ' ') ? q + 1 : q;
    }
    return;
  }
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

static FuiPage fui_current      = PG_NONE;
static FuiPage fui_pending      = PG_NONE;   // page we commanded, awaiting 0x66
static bool     fui_boot_done   = false;
static uint32_t fui_last_tick   = 0;
static uint32_t fui_probe_next  = 0;
static uint8_t  fui_probe_i     = 1;         // index into page table
static uint8_t  fui_probe_wait  = 0;         // 0x66 replies outstanding

FuiStats fui_stats = { 0, 0, 0 };

// EEPROM lane: settings.cpp reads/writes these (EEPROM V91)
uint32_t fui_eeprom_prints = 0, fui_eeprom_seconds = 0, fui_eeprom_filament_mm = 0;

static bool stats_printing      = false;
static float stats_last_e       = 0.0f;
static uint32_t stats_sec_acc   = 0;         // sub-second accumulator

// ---------------------------------------------------------------------------
// Serial primitives
// ---------------------------------------------------------------------------

void FUI::cmd(const char *s) { LCD_SERIAL_2.printf("%s\xff\xff\xff", s); }

void FUI::rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  LCD_SERIAL_2.printf("fill %u,%u,%u,%u,%u\xff\xff\xff", x, y, w, h, color);
}

void FUI::text(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
               const char *str, uint16_t color, FuiAlign align, uint16_t bco) {
  // xstr x,y,w,h,font,bco,pco,xcen,ycen,"str"
  if (!str) str = "";
  LCD_SERIAL_2.printf("xstr %u,%u,%u,%u,0,%u,%u,%u,1,\"%s\"\xff\xff\xff",
                      x, y, w, h, bco, color, (uint8_t)align, str);
}

void FUI::hline(uint16_t y, uint16_t x0, uint16_t x1, uint16_t color) {
  LCD_SERIAL_2.printf("line %u,%u,%u,%u,%u\xff\xff\xff", x0, y, x1, y, color);
}

void FUI::button(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 const char *label, bool highlight, uint16_t bco, uint16_t fco) {
  rect(x, y, w, h, highlight ? FUI_CARD2 : bco);
  rect(x, y, w, 1, FUI_BORDER);
  rect(x, y + h - 1, w, 1, FUI_BORDER);
  rect(x, y, 1, h, FUI_BORDER);
  rect(x + w - 1, y, 1, h, FUI_BORDER);
  text(x + 2, y, w - 4, h, label, highlight ? FUI_ACCENT : fco, FUI_CENTER, highlight ? FUI_CARD2 : bco);
}

void FUI::fill_bg() { rect(0, 0, FUI_SCREEN_W, FUI_SCREEN_H, FUI_BG); }

void FUI::title(const char *s) {
  text(8, 6, 256, 30, s, FUI_TEXT, FUI_LEFT, FUI_BG);
  hline(40, 8, 264, FUI_BORDER);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void fmt_temp(char *buf, size_t n, int16_t cur, int16_t tgt) {
  if (tgt > 0) snprintf(buf, n, "%d/%d\xB0" "C", (int)cur, (int)tgt);
  else         snprintf(buf, n, "%d\xB0" "C", (int)cur);
}

static void fmt_dur(char *buf, size_t n, uint32_t s) {
  snprintf(buf, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600),
           (unsigned long)((s % 3600) / 60), (unsigned long)(s % 60));
}

static bool fui_is_printing() { return ExtUI::isPrintingFromMedia(); }
static bool fui_is_paused()  { return ExtUI::isPrintingFromMediaPaused(); }

static uint8_t fui_progress() {
  #if HAS_MEDIA
    return card.isPrinting() ? card.percentDone() : 0;
  #else
    return 0;
  #endif
}

static const char *fui_state_str() {
  if (fui_is_printing()) return fui_is_paused() ? "PAUSED" : "PRINTING";
  if (print_job_timer.isRunning()) return "BUSY";
  if (marlin_state == MarlinState::MF_STOPPED) return "STOPPED";
  return "READY";
}

static uint16_t fui_state_color() {
  if (fui_is_printing()) return fui_is_paused() ? FUI_AMBER : FUI_GREEN;
  if (marlin_state == MarlinState::MF_STOPPED) return FUI_RED;
  return FUI_MUTED;
}

// ---------------------------------------------------------------------------
// Boot probe
// ---------------------------------------------------------------------------

#define FUI_PROBE_DELAY_MS 10000   // let the fork's own boot flow finish first

void FUI::init() {
  // enable page-change notifications: 0x66 <idx> FF FF FF on every page switch
  cmd("sendme");
  // pull persisted stats (settings.load() ran before display init)
  fui_stats.prints = fui_eeprom_prints;
  fui_stats.seconds = fui_eeprom_seconds;
  fui_stats.filament_mm = fui_eeprom_filament_mm;
  fui_boot_done = false;
  fui_probe_i = 1;
  fui_probe_next = millis() + FUI_PROBE_DELAY_MS;
  fui_probe_wait = 0;
  fui_current = PG_NONE;
  fui_pending = PG_NONE;
}

static void fui_probe_start() {
  // backlight off: the page probe riffle stays invisible
  LCD_SERIAL_2.printf("dim=0\xff\xff\xff");
}

static void fui_probe_step() {
  static bool dimmed = false;
  if (fui_boot_done) return;
  if (!ELAPSED(millis(), fui_probe_next)) return;

  if (!dimmed) { dimmed = true; fui_probe_start(); }

  if (fui_probe_i < PG_COUNT) {
    LCD_SERIAL_2.printf("page %s\xff\xff\xff", fui_page_names[fui_probe_i]);
    fui_pending = (FuiPage)fui_probe_i;
    fui_probe_wait = 1;
    fui_probe_i++;
    fui_probe_next = millis() + 70;
  }
  else {
    // probe complete -> restore backlight and show the main screen
    fui_boot_done = true;
    LCD_SERIAL_2.printf("dim=50\xff\xff\xff");
    FUI::goto_name("main");
  }
}

bool FUI::booting() { return !fui_boot_done; }

// ---------------------------------------------------------------------------
// Page switching
// ---------------------------------------------------------------------------

void FUI::on_page_index(uint8_t idx) {
  if (idx < 128 && fui_idx_to_page[idx] != PG_NONE) {
    const FuiPage pg = (FuiPage)fui_idx_to_page[idx];
    // unsolicited switch to a different page than we think is showing?
    if (fui_boot_done && pg != fui_current) {
      fui_current = pg;
      draw_page();
    }
    return;
  }
  // unknown index: learn it if we know what we just asked for
  if (fui_pending != PG_NONE) {
    if (idx < 128) fui_idx_to_page[idx] = fui_pending;
    fui_page_to_idx[fui_pending] = idx;
    // during the boot probe: learn silently, no drawing
    if (fui_boot_done && fui_current == fui_pending) {
      // goto_name() already drew - nothing to do
    }
    fui_pending = PG_NONE;
  }
  // else: stray notification, ignore
}

void FUI::goto_name(const char *name) {
  for (uint8_t p = 1; p < PG_COUNT; p++) {
    if (!strcmp(fui_page_names[p], name)) {
      fui_pending = (FuiPage)p;
      fui_current = (FuiPage)p;
      LCD_SERIAL_2.printf("page %s\xff\xff\xff", name);
      // draw immediately: the commands queue behind the page switch and
      // cover the stock art the moment the new page loads
      draw_page();
      return;
    }
  }
  // unknown page name: switch without FractalUI chrome
  LCD_SERIAL_2.printf("page %s\xff\xff\xff", name);
}

void FUI::page_index(uint8_t idx) {
  // stock mechanism: write ExchangePageBase+idx to the page-switch VP
  rtscheck.RTS_SndData(ExchangePageBase + idx, ExchangepageAddr);
  const uint8_t pg = (idx < 128) ? fui_idx_to_page[idx] : PG_NONE;
  if (pg != PG_NONE) {
    fui_pending = (FuiPage)pg;
    // draw happens on 0x66 confirmation
  }
  // unknown index: the 0x66 will tell us which page we're on; if even that
  // doesn't arrive (sendme lost), we simply don't redraw - stock page shows.
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------

static void fui_stats_tick(uint32_t now_ms) {
  const bool printing = fui_is_printing() && !fui_is_paused();
  if (printing) {
    stats_sec_acc += now_ms - fui_last_tick;
    while (stats_sec_acc >= 1000) {
      stats_sec_acc -= 1000;
      fui_stats.seconds++;
    }
    // filament estimate: positive E deltas only (skips retracts/G92 resets)
    const float e = current_position.e;
    const float d = e - stats_last_e;
    if (d > 0.0f && d < 100.0f) fui_stats.filament_mm += (uint32_t)d;
    stats_last_e = e;
  }
  else {
    stats_last_e = current_position.e;
  }

  // print start / end edges
  static bool was_printing = false;
  if (printing && !was_printing) fui_stats.prints++;
  if (!fui_is_printing() && was_printing) {
    // print finished: persist
    fui_eeprom_prints = fui_stats.prints;
    fui_eeprom_seconds = fui_stats.seconds;
    fui_eeprom_filament_mm = fui_stats.filament_mm;
    #if ENABLED(EEPROM_SETTINGS)
      settings.save();
    #endif
  }
  was_printing = fui_is_printing();
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void FUI::tick() {
  fui_probe_step();
  if (!fui_boot_done) return;

  const uint32_t now = millis();
  if (PENDING(now, fui_last_tick + 250)) return;

  fui_stats_tick(now);
  fui_last_tick = now;
  draw_dynamic();
}

// ---------------------------------------------------------------------------
// Page drawing
// ---------------------------------------------------------------------------

void FUI::generic_fallback() {
  fill_bg();
  title(fui_current < PG_COUNT && fui_page_names[fui_current]
        ? fui_page_names[fui_current] : "?");
  text(8, 60, 256, 30, "screen not skinned yet", FUI_MUTED, FUI_CENTER, FUI_BG);
}

// --- main ------------------------------------------------------------------

static void draw_main_static() {
  FUI::fill_bg();
  // menu buttons on the stock hotspots
  FUI::button( 17, 105, 105,  98, "PRINT",    false, FUI_CARD, FUI_TEXT);
  FUI::button(145, 103, 105, 102, "PREPARE",  false, FUI_CARD, FUI_TEXT);
  FUI::button( 19, 231, 103, 102, "SETTINGS", false, FUI_CARD, FUI_TEXT);
  FUI::button(152, 233, 101,  93, "LEVEL",    false, FUI_CARD, FUI_TEXT);
  FUI::text(8, 372, 256, 24, "FractalUI", FUI_MUTED, FUI_RIGHT, FUI_BG);
}

static void draw_main_dynamic() {
  char buf[48];
  FUI::text(8, 8, 160, 26, fui_state_str(), fui_state_color(), FUI_LEFT, FUI_BG);
  fmt_temp(buf, sizeof(buf), thermalManager.degHotend(0), thermalManager.degTargetHotend(0));
  FUI::text(8, 36, 130, 30, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  FUI::text(70, 36, 60, 30, "NOZ", FUI_MUTED, FUI_LEFT, FUI_BG);
  fmt_temp(buf, sizeof(buf), thermalManager.degBed(), thermalManager.degTargetBed());
  FUI::text(142, 36, 122, 30, buf, FUI_TEXT, FUI_RIGHT, FUI_BG);
  FUI::text(142, 8, 122, 24, "BED", FUI_MUTED, FUI_RIGHT, FUI_BG);
  snprintf(buf, sizeof(buf), "FAN %u%%", (unsigned)(thermalManager.fan_speed[0] * 100 / 255));
  FUI::text(8, 68, 130, 24, buf, FUI_MUTED, FUI_LEFT, FUI_BG);
  if (fui_is_printing()) {
    snprintf(buf, sizeof(buf), "%u%%", fui_progress());
    FUI::text(142, 68, 122, 24, buf, FUI_ACCENT, FUI_RIGHT, FUI_BG);
  }
}

// --- print dashboard (printpause) -------------------------------------------

static void draw_printpause_static() {
  FUI::fill_bg();
  FUI::title("PRINT");
  FUI::button(177,  60, 83, 50, "TUNE",  false, FUI_CARD, FUI_TEXT);
  FUI::button(176, 130, 82, 50, "PAUSE", false, FUI_CARD, FUI_TEXT);
  FUI::button(178, 194, 84, 51, "STOP",  false, FUI_CARD, FUI_RED);
  FUI::button(177, 260, 83, 50, "LED",   false, FUI_CARD, FUI_TEXT);
  // progress bar frame
  FUI::rect(14, 108, 244, 16, FUI_CARD);
  FUI::rect(14, 108, 244, 1, FUI_BORDER);
  FUI::rect(14, 123, 244, 1, FUI_BORDER);
}

static void draw_printpause_dynamic() {
  char buf[48];
  // filename
  FUI::text(8, 44, 160, 28, CardRecbuf.Cardshowfilename[0][0] ? (const char*)CardRecbuf.Cardshowfilename[0] : "-", FUI_TEXT, FUI_LEFT, FUI_BG);
  // percent big
  snprintf(buf, sizeof(buf), "%u%%", fui_progress());
  FUI::text(170, 40, 94, 30, buf, FUI_ACCENT, FUI_RIGHT, FUI_BG);
  // progress bar fill
  const uint8_t pct = fui_progress();
  FUI::rect(16, 111, 240, 10, FUI_BG);
  FUI::rect(16, 111, (uint16_t)(240UL * pct / 100), 10, FUI_ACCENT);
  // elapsed / remaining
  fmt_dur(buf, sizeof(buf), print_job_timer.duration());
  FUI::text(14, 134, 120, 26, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  FUI::text(14, 148, 120, 22, "ELAPSED", FUI_MUTED, FUI_LEFT, FUI_BG);
  if (pct > 0) {
    const uint32_t rem = print_job_timer.duration() * (100 - pct) / pct;
    fmt_dur(buf, sizeof(buf), rem);
    FUI::text(140, 134, 118, 26, buf, FUI_TEXT, FUI_RIGHT, FUI_BG);
    FUI::text(140, 148, 118, 22, "LEFT", FUI_MUTED, FUI_RIGHT, FUI_BG);
  }
  // temps + speeds + position (left column, under buttons)
  fmt_temp(buf, sizeof(buf), thermalManager.degHotend(0), thermalManager.degTargetHotend(0));
  FUI::text(14, 190, 150, 26, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  FUI::text(14, 212, 60, 22, "NOZ", FUI_MUTED, FUI_LEFT, FUI_BG);
  fmt_temp(buf, sizeof(buf), thermalManager.degBed(), thermalManager.degTargetBed());
  FUI::text(14, 238, 150, 26, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  FUI::text(14, 260, 60, 22, "BED", FUI_MUTED, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "SPD %d%%  FLW %d%%", (int)feedrate_percentage, (int)planner.flow_percentage[0]);
  FUI::text(14, 290, 240, 26, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "X%.1f Y%.1f Z%.2f",
           (double)current_position.x, (double)current_position.y, (double)current_position.z);
  FUI::text(14, 320, 240, 26, buf, FUI_MUTED, FUI_LEFT, FUI_BG);
  // pause button label reflects state
  FUI::button(176, 130, 82, 50, fui_is_paused() ? "RESUME" : "PAUSE", false, FUI_CARD, FUI_TEXT);
}

// --- file browser -----------------------------------------------------------

static uint8_t file_base(FuiPage pg) {
  switch (pg) {
    case PG_PRINTFILES: return 0;   // rows 0-7
    case PG_FILE1: return 1;        // rows 1-5
    case PG_FILE2: return 6;
    case PG_FILE3: return 11;
    case PG_FILE4: return 16;
    case PG_FILE5: return 21;
    default: return 0;
  }
}

static void draw_files_static() {
  FUI::fill_bg();
  FUI::title("FILES");
  const bool isPrintFiles = (fui_current == PG_PRINTFILES);
  const uint8_t rows = isPrintFiles ? 8 : 5;
  const uint16_t ys[8] = { 61, 104, 148, 193, 237, 281, 326, 370 };
  char buf[48];
  for (uint8_t r = 0; r < rows; r++) {
    FUI::rect(12, ys[r] - 3, 248, 38, FUI_CARD);
    FUI::rect(12, ys[r] - 3, 248, 1, FUI_BORDER);
    snprintf(buf, sizeof(buf), "%u", file_base(fui_current) + r + 1);
    FUI::text(16, ys[r] - 3, 24, 38, buf, FUI_MUTED, FUI_LEFT, FUI_CARD);
  }
  if (isPrintFiles) {
    FUI::button(19, 424, 109, 36, "PREV", false, FUI_CARD, FUI_TEXT);
    FUI::button(147, 425, 105, 35, "NEXT", false, FUI_CARD, FUI_TEXT);
  }
  else {
    FUI::button(19, 403, 107, 49, "PREV", false, FUI_CARD, FUI_TEXT);
    FUI::button(146, 403, 107, 50, "NEXT", false, FUI_CARD, FUI_TEXT);
  }
}

static void draw_files_dynamic() {
  const bool isPrintFiles = (fui_current == PG_PRINTFILES);
  const uint8_t rows = isPrintFiles ? 8 : 5;
  const uint16_t ys[8] = { 61, 104, 148, 193, 237, 281, 326, 370 };
  char name[TEXTBYTELEN + 1];
  for (uint8_t r = 0; r < rows; r++) {
    const uint8_t idx = file_base(fui_current) + r;
    strncpy(name, CardRecbuf.Cardshowfilename[idx], TEXTBYTELEN);
    name[TEXTBYTELEN] = 0;
    if (!name[0]) strcpy(name, " ");
    FUI::text(44, ys[r] - 3, 212, 38, name, FUI_TEXT, FUI_LEFT, FUI_CARD);
  }
}

// --- dialogs -----------------------------------------------------------------

static void dialog(const char *title, const char *msg,
                   const char *b1, const char *b2,
                   uint16_t x1, uint16_t y1, uint16_t w1, uint16_t h1,
                   uint16_t x2, uint16_t y2, uint16_t w2, uint16_t h2) {
  FUI::fill_bg();
  FUI::title(title);
  FUI::text(16, 140, 240, 60, msg, FUI_TEXT, FUI_CENTER, FUI_BG);
  if (b1) FUI::button(x1, y1, w1, h1, b1, false, FUI_CARD, FUI_ACCENT);
  if (b2) FUI::button(x2, y2, w2, h2, b2, false, FUI_CARD, FUI_TEXT);
}

// --- prepare (move / temp / filament) ----------------------------------------

static void draw_premove_static() {
  FUI::fill_bg();
  FUI::title("MOVE");
  // axis select
  FUI::button(22,  56, 69, 38, "X", false, FUI_CARD, FUI_TEXT);
  FUI::button(102, 52, 69, 47, "Y", false, FUI_CARD, FUI_TEXT);
  FUI::button(185, 54, 69, 38, "Z", false, FUI_CARD, FUI_TEXT);
  // jog pad
  FUI::button( 16, 185, 60, 60, "-", false, FUI_CARD, FUI_TEXT);
  FUI::button(201, 189, 60, 60, "+", false, FUI_CARD, FUI_TEXT);
  FUI::button( 16, 276, 60, 60, "+", false, FUI_CARD, FUI_TEXT);
  FUI::button(195, 271, 60, 60, "-", false, FUI_CARD, FUI_TEXT);
  FUI::button(107, 139, 60, 60, "OK", false, FUI_CARD, FUI_TEXT);   // Z home/confirm
  FUI::button(112, 236, 43, 42, "\xB7", false, FUI_CARD, FUI_MUTED);
  // step
  FUI::button(17, 128, 45, 40, "0.1", false, FUI_CARD, FUI_MUTED);
  FUI::button(212, 125, 43, 43, "10", false, FUI_CARD, FUI_MUTED);
  // extrude / retract
  FUI::button(15, 347, 45, 45, "E-", false, FUI_CARD, FUI_TEXT);
  FUI::button(215, 350, 40, 41, "E+", false, FUI_CARD, FUI_TEXT);
  // bottom row: heat / filament / back
  FUI::button(100, 320, 74, 40, "HOME", false, FUI_CARD, FUI_MUTED);
  FUI::button(100, 432, 73, 41, "HEAT", false, FUI_CARD, FUI_TEXT);
  FUI::button(191, 432, 75, 45, "FIL", false, FUI_CARD, FUI_TEXT);
}

static void draw_premove_dynamic() {
  char buf[48];
  snprintf(buf, sizeof(buf), "X %.1f  Y %.1f  Z %.2f",
           (double)current_position.x, (double)current_position.y, (double)current_position.z);
  FUI::text(16, 108, 240, 24, buf, FUI_ACCENT, FUI_CENTER, FUI_BG);
}

static void draw_pretemp_static() {
  FUI::fill_bg();
  FUI::title("HEAT");
  FUI::button( 25, 312, 101, 36, "PLA", false, FUI_CARD, FUI_TEXT);
  FUI::button(148, 310,  95, 42, "PETG", false, FUI_CARD, FUI_TEXT);
  FUI::button( 32, 363,  94, 42, "ABS", false, FUI_CARD, FUI_TEXT);
  FUI::button(159, 364,  89, 39, "TPU", false, FUI_CARD, FUI_TEXT);
  FUI::button(204, 80, 47, 41, "NOZ", false, FUI_CARD, FUI_MUTED);
  FUI::button(204, 164, 47, 48, "BED", false, FUI_CARD, FUI_MUTED);
  FUI::button(192, 432, 74, 41, "MOVE", false, FUI_CARD, FUI_TEXT);
}

static void draw_pretemp_dynamic() {
  char buf[48];
  fmt_temp(buf, sizeof(buf), thermalManager.degHotend(0), thermalManager.degTargetHotend(0));
  FUI::text(12, 90, 180, 30, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  FUI::text(12, 120, 180, 22, "NOZZLE", FUI_MUTED, FUI_LEFT, FUI_BG);
  fmt_temp(buf, sizeof(buf), thermalManager.degBed(), thermalManager.degTargetBed());
  FUI::text(12, 174, 180, 30, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  FUI::text(12, 204, 180, 22, "BED", FUI_MUTED, FUI_LEFT, FUI_BG);
}

static void draw_prefilament_static() {
  FUI::fill_bg();
  FUI::title("FILAMENT");
  FUI::button(157, 346, 95, 51, "LOAD", false, FUI_CARD, FUI_TEXT);
  FUI::button( 21, 347, 95, 51, "UNLOAD", false, FUI_CARD, FUI_TEXT);
  FUI::button( 99, 431, 78, 43, "HEAT", false, FUI_CARD, FUI_TEXT);
  FUI::text(16, 120, 240, 30, "Filament operations", FUI_MUTED, FUI_CENTER, FUI_BG);
}

// --- adjust (temps / tuning) --------------------------------------------------

static void draw_adjusttemp_static() {
  FUI::fill_bg();
  FUI::title("TEMP");
  FUI::button(  7,  64, 121, 36, "NOZZLE", false, FUI_CARD, FUI_MUTED);
  FUI::button(138,  60, 123, 38, "BED", false, FUI_CARD, FUI_MUTED);
  FUI::button( 17, 127, 71, 39, "-1", false, FUI_CARD, FUI_TEXT);
  FUI::button( 99, 129, 71, 39, "-10", false, FUI_CARD, FUI_TEXT);
  FUI::button(179, 129, 71, 39, "+10", false, FUI_CARD, FUI_TEXT);
  FUI::button( 20, 197, 35, 34, "\x2D", false, FUI_CARD, FUI_TEXT);   // -
  FUI::button(173, 196, 34, 35, "\x2B", false, FUI_CARD, FUI_TEXT);   // +
  FUI::button(216, 196, 34, 34, "OK", false, FUI_CARD, FUI_ACCENT);
  FUI::button( 15, 307,  96, 57, "COOL", false, FUI_CARD, FUI_TEXT);
  FUI::button(154, 306, 100, 56, "PREHEAT", false, FUI_CARD, FUI_TEXT);
  FUI::button( 94, 423,  84, 57, "BACK", false, FUI_CARD, FUI_MUTED);
}

static void draw_adjusttemp_dynamic() {
  char buf[48];
  const bool nozzle = true; // fork tracks selection; v1 shows both
  fmt_temp(buf, sizeof(buf), thermalManager.degHotend(0), thermalManager.degTargetHotend(0));
  FUI::text(12, 230, 120, 34, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  FUI::text(12, 262, 120, 22, "NOZZLE", FUI_MUTED, FUI_LEFT, FUI_BG);
  fmt_temp(buf, sizeof(buf), thermalManager.degBed(), thermalManager.degTargetBed());
  FUI::text(140, 230, 120, 34, buf, FUI_TEXT, FUI_RIGHT, FUI_BG);
  FUI::text(140, 262, 120, 22, "BED", FUI_MUTED, FUI_RIGHT, FUI_BG);
  (void)nozzle;
}

static void draw_adjustspeed_static() {
  FUI::fill_bg();
  FUI::title("TUNE");
  FUI::button( 12, 61, 80, 35, "SPD", false, FUI_CARD, FUI_MUTED);
  FUI::button( 98, 61, 80, 35, "FLW", false, FUI_CARD, FUI_MUTED);
  FUI::button(185, 61, 80, 35, "MORE", false, FUI_CARD, FUI_MUTED);
  FUI::button( 19, 125, 71, 36, "-1", false, FUI_CARD, FUI_TEXT);
  FUI::button(102, 125, 71, 36, "-10", false, FUI_CARD, FUI_TEXT);
  FUI::button(182, 125, 71, 36, "+10", false, FUI_CARD, FUI_TEXT);
  FUI::button( 18, 214, 45, 43, "\x2D", false, FUI_CARD, FUI_TEXT);
  FUI::button(166, 215, 43, 41, "\x2B", false, FUI_CARD, FUI_TEXT);
  FUI::button(212, 215, 43, 41, "OK", false, FUI_CARD, FUI_ACCENT);
  FUI::button(  0, 425,  89, 55, "BACK", false, FUI_CARD, FUI_MUTED);
  FUI::button(183, 426,  89, 55, "TEMP", false, FUI_CARD, FUI_TEXT);
}

static void draw_adjustspeed_dynamic() {
  char buf[48];
  snprintf(buf, sizeof(buf), "SPEED  %d%%", (int)feedrate_percentage);
  FUI::text(16, 170, 240, 34, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "FLOW   %d%%", (int)planner.flow_percentage[0]);
  FUI::text(16, 204, 240, 34, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "ACC %lu  JERK %.1f",
           (unsigned long)planner.settings.max_acceleration_mm_per_s2[0],
           (double)planner.max_jerk.x);
  FUI::text(16, 282, 240, 26, buf, FUI_MUTED, FUI_LEFT, FUI_BG);
}

// --- settings / stats ----------------------------------------------------------

static void draw_set_static() {
  FUI::fill_bg();
  FUI::title("SETTINGS");
  const uint16_t rows[6] = { 60, 118, 176, 234, 292, 350 };
  const char *labels[6] = { "PRESETS", "FAN", "LED", "FILAMENT", "STATS", "RESET" };
  for (uint8_t i = 0; i < 6; i++) {
    FUI::rect(8, rows[i] - 4, 256, 44, FUI_CARD);
    FUI::rect(8, rows[i] - 4, 256, 1, FUI_BORDER);
    FUI::text(20, rows[i] - 4, 200, 44, labels[i], FUI_TEXT, FUI_LEFT, FUI_CARD);
    FUI::text(232, rows[i] - 4, 24, 44, ">", FUI_MUTED, FUI_LEFT, FUI_CARD);
  }
  FUI::button(8, 409, 257, 39, "BACK", false, FUI_CARD, FUI_MUTED);
}

static void draw_set_dynamic() { }

static void draw_information_static() {
  FUI::fill_bg();
  FUI::title("STATS");
}

static void draw_information_dynamic() {
  char buf[64];
  FUI::text(12, 60, 130, 28, "Total prints", FUI_MUTED, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)fui_stats.prints);
  FUI::text(130, 60, 130, 28, buf, FUI_TEXT, FUI_RIGHT, FUI_BG);
  FUI::text(12, 95, 130, 28, "Print time", FUI_MUTED, FUI_LEFT, FUI_BG);
  fmt_dur(buf, sizeof(buf), fui_stats.seconds);
  FUI::text(130, 95, 130, 28, buf, FUI_TEXT, FUI_RIGHT, FUI_BG);
  FUI::text(12, 130, 130, 28, "Filament", FUI_MUTED, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "%lum", (unsigned long)(fui_stats.filament_mm / 1000));
  FUI::text(130, 130, 130, 28, buf, FUI_TEXT, FUI_RIGHT, FUI_BG);
  FUI::hline(175, 12, 260, FUI_BORDER);
  FUI::text(12, 190, 130, 28, "Firmware", FUI_MUTED, FUI_LEFT, FUI_BG);
  FUI::text(130, 190, 130, 28, SHORT_BUILD_VERSION, FUI_TEXT, FUI_RIGHT, FUI_BG);
  FUI::text(12, 225, 130, 28, "UI", FUI_MUTED, FUI_LEFT, FUI_BG);
  FUI::text(130, 225, 130, 28, "FractalUI v1", FUI_TEXT, FUI_RIGHT, FUI_BG);
  FUI::text(12, 260, 130, 28, "Board", FUI_MUTED, FUI_LEFT, FUI_BG);
  FUI::text(130, 260, 130, 28, "MKS E3D V2", FUI_TEXT, FUI_RIGHT, FUI_BG);
  // maintenance hints
  FUI::hline(300, 12, 260, FUI_BORDER);
  FUI::text(12, 315, 248, 26, "MAINTENANCE", FUI_MUTED, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "Nozzle: %lu prints", (unsigned long)fui_stats.prints);
  FUI::text(12, 345, 248, 26, buf, fui_stats.prints > 500 ? FUI_AMBER : FUI_TEXT, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "Bed lube: %lu h", (unsigned long)(fui_stats.seconds / 3600));
  FUI::text(12, 375, 248, 26, buf, FUI_TEXT, FUI_LEFT, FUI_BG);
}

// --- leveling -------------------------------------------------------------------

static void draw_leveling_static() {
  FUI::fill_bg();
  FUI::title("AUTO LEVEL");
  FUI::rect(14, 120, 244, 16, FUI_CARD);
}

static void draw_leveling_dynamic() {
  char buf[48];
  extern uint8_t showcount;
  const uint8_t total = GRID_MAX_POINTS;
  const uint8_t done = showcount > total ? total : showcount;
  FUI::text(8, 44, 256, 30, "Leveling bed...", FUI_TEXT, FUI_LEFT, FUI_BG);
  snprintf(buf, sizeof(buf), "%u / %u", done, total);
  FUI::text(170, 80, 94, 30, buf, FUI_ACCENT, FUI_RIGHT, FUI_BG);
  FUI::rect(16, 123, 240, 10, FUI_BG);
  if (total) FUI::rect(16, 123, (uint16_t)(240UL * done / total), 10, FUI_ACCENT);
}

static void draw_leveldata_static() {
  FUI::fill_bg();
  FUI::title("LEVEL OK");
  FUI::button( 11, 142, 73, 44, "\x2D", false, FUI_CARD, FUI_TEXT);
  FUI::button( 94, 144, 73, 44, "Z0", false, FUI_CARD, FUI_MUTED);
  FUI::button(177, 142, 73, 44, "\x2B", false, FUI_CARD, FUI_TEXT);
  FUI::button( 24, 198, 51, 47, "RESET", false, FUI_CARD, FUI_TEXT);
  FUI::button(197, 197, 50, 46, "SAVE", false, FUI_CARD, FUI_TEXT);
  FUI::button(  3,   3, 85, 36, "DONE", false, FUI_CARD, FUI_ACCENT);
}

static void draw_leveldata_dynamic() {
  char buf[48];
  snprintf(buf, sizeof(buf), "%.2f mm", (double)probe.offset.z);
  FUI::text(46, 90, 180, 36, buf, FUI_ACCENT, FUI_CENTER, FUI_BG);
  FUI::text(46, 126, 180, 22, "Z OFFSET", FUI_MUTED, FUI_CENTER, FUI_BG);
}

// --- misc ------------------------------------------------------------------------

static void draw_wait_dynamic() {
  char buf[48];
  fmt_temp(buf, sizeof(buf), thermalManager.degHotend(0), thermalManager.degTargetHotend(0));
  FUI::text(12, 120, 248, 30, buf, FUI_TEXT, FUI_CENTER, FUI_BG);
  FUI::text(12, 150, 248, 22, "NOZZLE", FUI_MUTED, FUI_CENTER, FUI_BG);
  fmt_temp(buf, sizeof(buf), thermalManager.degBed(), thermalManager.degTargetBed());
  FUI::text(12, 190, 248, 30, buf, FUI_TEXT, FUI_CENTER, FUI_BG);
  FUI::text(12, 220, 248, 22, "BED", FUI_MUTED, FUI_CENTER, FUI_BG);
}

// ---------------------------------------------------------------------------
// Dispatcher
// ---------------------------------------------------------------------------

void FUI::draw_page() {
  // hide stock visual components so their VP-driven repaints can't bleed
  // through the overlay (touchable components stay live)
  if (fui_current < PG_COUNT) fui_send_hides(fui_page_names[fui_current]);
  draw_static();
  draw_dynamic();
}

void FUI::draw_static() {
  switch (fui_current) {
    case PG_MAIN:         draw_main_static(); break;
    case PG_PRINTFILES:
    case PG_FILE1: case PG_FILE2: case PG_FILE3: case PG_FILE4: case PG_FILE5:
                          draw_files_static(); break;
    case PG_PRINTPAUSE:   draw_printpause_static(); break;
    case PG_PREMOVE:      draw_premove_static(); break;
    case PG_PRETEMP:      draw_pretemp_static(); break;
    case PG_PREFILAMENT:  draw_prefilament_static(); break;
    case PG_ADJUSTTEMP:   draw_adjusttemp_static(); break;
    case PG_ADJUSTSPEED:  draw_adjustspeed_static(); break;
    case PG_SET:          draw_set_static(); break;
    case PG_INFORMATION:  draw_information_static(); break;
    case PG_LEVELING:     draw_leveling_static(); break;
    case PG_LEVELDATA:    draw_leveldata_static(); break;
    case PG_ASKPRINT:
      dialog("PRINT?", CardRecbuf.Cardshowfilename[0][0] ? (const char*)CardRecbuf.Cardshowfilename[0] : "",
             "PRINT", "BACK", 26, 322, 110, 40, 140, 324, 110, 41);
      break;
    case PG_PAUSECONFIRM: dialog("PAUSE?", "Pause the print?", "NO", "PAUSE", 28, 320, 105, 44, 145, 321, 101, 46); break;
    case PG_RESUMECONFIRM: dialog("STOP?", "Stop the print?", "RESUME", "STOP", 30, 320, 105, 47, 0, 0, 0, 0); break;
    case PG_CANCLEHEAT:   dialog("HEATING", "Cancel heating?", "CANCEL", "WAIT", 29, 321, 103, 45, 141, 320, 104, 47); break;
    case PG_NOFILAMENT:
    case PG_NOFILAMENTPUSH:
      dialog("FILAMENT", "Insert filament, then continue", "PURGE", "STOP", 26, 324, 110, 38, 140, 326, 110, 36); break;
    case PG_FILAMENTRESUME:
      dialog("FILAMENT", "Filament loaded", "RESUME", "PURGE+", 15, 404, 97, 50, 158, 208, 96, 57); break;
    case PG_HEATFILAMENT: dialog("HEATING", "Heating for filament ops...", nullptr, nullptr, 0, 0, 0, 0, 0, 0, 0, 0); break;
    case PG_AUTOHOME:     FUI::fill_bg(); FUI::title("HOMING"); break;
    case PG_WAIT:         FUI::fill_bg(); FUI::title("BUSY"); break;
    case PG_TEMPSET:      FUI::fill_bg(); FUI::title("PRESETS"); break;
    case PG_TEMPSETVALUE: FUI::fill_bg(); FUI::title("PRESET"); break;
    case PG_MULTISET:     FUI::fill_bg(); FUI::title("ADVANCED"); break;
    case PG_WARNRDLEVEL:  dialog("LEVEL", "Bed should be clean & cold", "START", "BACK", 25, 322, 106, 42, 0, 0, 0, 0); break;
    case PG_NOSDCARD:     dialog("NO CARD", "Insert SD card", "RETRY", "BACK", 30, 322, 98, 40, 140, 322, 103, 42); break;
    case PG_PRINTFISH:    dialog("DONE", "Print complete", "OK", nullptr, 141, 269, 105, 46, 0, 0, 0, 0); break;
    case PG_CONTINUEPRINT: dialog("POWER LOSS", "Resume interrupted print?", "RESUME", "CANCEL", 30, 322, 100, 41, 143, 320, 102, 46); break;
    case PG_FACTORYSETTING: dialog("RESET", "Restore all settings?", "RESET", "BACK", 31, 323, 103, 43, 148, 323, 97, 43); break;
    case PG_LANGUAGE:     FUI::fill_bg(); FUI::title("LANGUAGE"); break;
    case PG_ERR_NOZZLEHEAT: dialog("ERROR", "Nozzle heating failed", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_BEDHEAT:   dialog("ERROR", "Bed heating failed", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_HEATFAIL:  dialog("ERROR", "Heating failed (runaway)", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_NOZZLEOVER: dialog("ERROR", "Nozzle over temp", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_NOZZLEUNDE: dialog("ERROR", "Nozzle under min temp", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_BEDOVER:   dialog("ERROR", "Bed over temp", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_BEDUNDER:  dialog("ERROR", "Bed under min temp", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_HOMEFAIL:  dialog("ERROR", "Homing failed", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_PROBEFAIL: dialog("ERROR", "Bed probing failed", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_SDREAD:    dialog("ERROR", "SD read failed", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    case PG_ERR_SDWRITE:   dialog("ERROR", "SD write failed", nullptr, "OK", 0, 0, 0, 0, 0, 322, 101, 45); break;
    default: generic_fallback(); break;
  }
}

void FUI::draw_dynamic() {
  switch (fui_current) {
    case PG_MAIN:         draw_main_dynamic(); break;
    case PG_PRINTFILES:
    case PG_FILE1: case PG_FILE2: case PG_FILE3: case PG_FILE4: case PG_FILE5:
                          draw_files_dynamic(); break;
    case PG_PRINTPAUSE:   draw_printpause_dynamic(); break;
    case PG_PREMOVE:      draw_premove_dynamic(); break;
    case PG_PRETEMP:      draw_pretemp_dynamic(); break;
    case PG_ADJUSTTEMP:   draw_adjusttemp_dynamic(); break;
    case PG_ADJUSTSPEED:  draw_adjustspeed_dynamic(); break;
    case PG_INFORMATION:  draw_information_dynamic(); break;
    case PG_LEVELING:     draw_leveling_dynamic(); break;
    case PG_LEVELDATA:    draw_leveldata_dynamic(); break;
    case PG_WAIT:         draw_wait_dynamic(); break;
    default: break;
  }
}

#endif // RTS_AVAILABLE
