/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2026 fractal-l
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "../../../../inc/MarlinConfig.h"

#if ENABLED(TJC_AVAILABLE)

#include "fractalui.h"

  /**
   * tjc_page("name") switches the TJC screen to a page by name.
   *
   * With FractalUI enabled the switch is routed through FUI::goto_name()
   * so the page is immediately covered with the dark overlay UI and kept
   * in sync with the firmware-side page state.
   */
  FORCE_INLINE void tjc_page(const char * const page) {
    #if ENABLED(RTS_AVAILABLE)
      FUI::goto_name(page);
    #else
      LCD_SERIAL_2.printf("page %s", page);
      LCD_SERIAL_2.printf("\xff\xff\xff");
    #endif
  }

#endif
