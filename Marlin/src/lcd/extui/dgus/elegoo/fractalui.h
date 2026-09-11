/**
 * FractalUI — firmware-rendered dark UI overlay for the Elegoo TJC screen.
 *
 * The stock screen firmware (Elegoo V1.4.2) is used unchanged. FractalUI
 * covers every page with a dark background and draws the whole interface
 * over serial (xstr / fill / line). Touch comes from the stock hotspots,
 * which emit the stock 5A A5 key frames handled by the existing fork code.
 *
 * Coordinate space: 272 x 480 portrait (rotated 90° on the physical panel).
 */
#pragma once

#include "../../../../inc/MarlinConfig.h"

#if ENABLED(RTS_AVAILABLE)

#define FUI_SCREEN_W 272
#define FUI_SCREEN_H 480

// Text alignment for FUI::text()
enum FuiAlign : uint8_t { FUI_LEFT = 0, FUI_CENTER = 1, FUI_RIGHT = 2 };

class FUI {
public:
  // Boot: enable sendme, learn page indices, then show main.
  static void init();
  // Called from the display loop; rate-limits itself (~4 Hz).
  static void tick();
  // Switch by page name (e.g. "main"); draws after 0x66 confirmation.
  static void goto_name(const char *name);
  // Stock ExchangePageBase+N switch. Sends the VP write; draws on the
  // 0x66 confirmation using the learned index table.
  static void page_index(uint8_t idx);
  // 0x66 <idx> received from the screen.
  static void on_page_index(uint8_t idx);
  // True while the boot probe is still running (keys ignored).
  static bool booting();

  // ---- draw primitives (RGB565 colors) ----
  static void cmd(const char *s);                    // raw command + terminator
  static void rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
  static void text(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                   const char *str, uint16_t color, FuiAlign align, uint16_t bco);
  static void hline(uint16_t y, uint16_t x0, uint16_t x1, uint16_t color);
  static void button(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     const char *label, bool highlight, uint16_t bco, uint16_t fco);

private:
  static void draw_page();           // full draw of the current page
  static void draw_static();         // chrome (title, buttons)
  static void draw_dynamic();        // live values

 public:
  static void fill_bg();
  static void title(const char *s);
  static void generic_fallback();    // unskinned page
};

// Palette (RGB565)
#define FUI_RGB565(r,g,b) ((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3))
constexpr uint16_t FUI_BG     = FUI_RGB565(0x0D,0x11,0x17);
constexpr uint16_t FUI_CARD   = FUI_RGB565(0x16,0x1B,0x22);
constexpr uint16_t FUI_CARD2  = FUI_RGB565(0x21,0x26,0x2D);
constexpr uint16_t FUI_BORDER = FUI_RGB565(0x30,0x36,0x3D);
constexpr uint16_t FUI_TEXT   = FUI_RGB565(0xE6,0xED,0xF3);
constexpr uint16_t FUI_MUTED  = FUI_RGB565(0x8B,0x94,0x9E);
constexpr uint16_t FUI_ACCENT = FUI_RGB565(0x58,0xA6,0xFF);
constexpr uint16_t FUI_RED    = FUI_RGB565(0xF8,0x51,0x49);
constexpr uint16_t FUI_GREEN  = FUI_RGB565(0x3F,0xB9,0x50);
constexpr uint16_t FUI_AMBER  = FUI_RGB565(0xD2,0x99,0x22);

// Runtime stats (persisted in EEPROM, see settings.cpp)
typedef struct { uint32_t prints, seconds, filament_mm; } FuiStats;
extern FuiStats fui_stats;
extern uint32_t fui_eeprom_prints, fui_eeprom_seconds, fui_eeprom_filament_mm;

#endif // RTS_AVAILABLE
