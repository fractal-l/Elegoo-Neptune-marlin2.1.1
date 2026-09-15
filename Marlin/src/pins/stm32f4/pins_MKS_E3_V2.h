/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2021 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
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

#define DISABLE_DEBUG false // DISABLE_(DEBUG|JTAG) is not supported for STM32F4.
#define ALLOW_STM32F4

//
// Limit Switches 
//
#define X_DIAG_PIN                          PA13    // PA13   X-;//PC14 Z+
#define Y_DIAG_PIN                          PB8
#define Z_DIAG_PIN                          PC13

#define X_STOP_PIN                        X_DIAG_PIN
#define Y_STOP_PIN                        Y_DIAG_PIN
#define Z_MIN_PIN                         Z_DIAG_PIN

//
// Z Probe must be this pin
//
#define Z_MIN_PROBE_PIN                     PA8  // PROBE

//
// Temperature Sensors
//
#define TEMP_0_PIN                          PC1   // TH1
#define TEMP_BED_PIN                        PC0   // TB1

//
// Steppers
//
#define X_ENABLE_PIN                        PD2
#define X_STEP_PIN                          PC12
#define X_DIR_PIN                           PB3

#define Y_ENABLE_PIN                        PC10
#define Y_STEP_PIN                          PC11
#define Y_DIR_PIN                           PA15

#define Z_ENABLE_PIN                        PC8
#define Z_STEP_PIN                          PC7
#define Z_DIR_PIN                           PC9

#define E0_ENABLE_PIN                       PC6
#define E0_STEP_PIN                         PB10
#define E0_DIR_PIN                          PB1


//
// LED
//
#define LED3_PIN                            PB9    //顶灯

//
// BEEPER
//
#define BEEPER_PIN                          PC15   //蜂鸣器

//
// Auto fans
//
// Numeric form of PB0 (digital pin 16): the "A8" analog alias used by the
// variant for PB0 is not a preprocessor macro, which breaks pin-equality
// checks like _HAS_FAN / the auto-fan sanity check (both pins would
// evaluate to 0 and compare equal).
#define AUTO_FAN_PIN                        16     // PB0 - FAN2 (hotend auto fan)
#ifndef E0_AUTO_FAN_PIN
  #define E0_AUTO_FAN_PIN           AUTO_FAN_PIN
#endif

//
// Heaters / Fans
//
#define HEATER_0_PIN                        PA6    // "HE"
#define HEATER_BED_PIN                      PA5    // "HB"
#define FAN0_PIN                            7      // PA7 - FAN1 (part cooling fan; numeric, see AUTO_FAN_PIN note)

//
// Filament Runout Sensor
//
#define CHECKFILEMENT0_PIN                PB4

// Use one of these or SDCard-based Emulation will be used
// I2C_EEPROM deliberately NOT selected: 11x11 UBL mesh writes over slow I2C
// trip the watchdog (reboot mid-save, corrupted store, nothing ever persists;
// cf. upstream #18219 E3D "can't save at all", #21436). FLASH emulation is
// also unusable (81%-full F401 has no free 128KB sector). Settings live in
// "eeprom.dat" on the SD card instead - keep a card inserted at boot.
#define SDCARD_EEPROM_EMULATION                   // Use SD-card file EEPROM emulation
#define MARLIN_EEPROM_SIZE                0x2000  // 8KB store (file, no chip constraint)
#define I2C_SCL_PIN                       PB6
#define I2C_SDA_PIN                       PB7

// Оn the servos connector
#ifndef FIL_RUNOUT_PIN
  #define FIL_RUNOUT_PIN                       CHECKFILEMENT0_PIN
#endif




//
// Onboard SD card
//
// detect pin doesn't work when ONBOARD and NO_SD_HOST_DRIVE disabled
#ifndef SDCARD_CONNECTION
  #define SDCARD_CONNECTION              ONBOARD
#endif
#if SD_CONNECTION_IS(ONBOARD)
  #define ENABLE_SPI3
  #define SD_SS_PIN                              PB12
  #define SD_SCK_PIN                        PB13
  #define SD_MISO_PIN                       PB14
  #define SD_MOSI_PIN                       PB15
  #define SD_DETECT_PIN                     PC3
  #define SD_SPI_SPEED                      SPI_FULL_SPEED        
#endif

// Ignore temp readings during development.
//#define BOGUS_TEMPERATURE_GRACE_PERIOD    2000

