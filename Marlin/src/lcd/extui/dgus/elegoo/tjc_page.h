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

  /**
   * tjc_page("name") sends a TJC/DGUS-II page-change command over
   * LCD_SERIAL_2: "page name" followed by the 0xFF 0xFF 0xFF terminator.
   *
   * Replaces the repeated two-line pattern:
   *   LCD_SERIAL_2.printf("page name");
   *   LCD_SERIAL_2.printf("\xff\xff\xff");
   *
   * Include after inc/MarlinConfig.h (and the HAL) so LCD_SERIAL_2
   * and FORCE_INLINE are defined.
   */
  FORCE_INLINE void tjc_page(const char * const page) {
    LCD_SERIAL_2.printf("page %s", page);
    LCD_SERIAL_2.printf("\xff\xff\xff");
  }

#endif
