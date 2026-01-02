/*
Copyright 2023 Morgan Venable @_claussen

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once
// This is used by our user's keymaps for conditionals.
#define SVALBOARD

// Keyboard UID for profile matching (same as original Vial UID for .vil compatibility)
#define VIABLE_KEYBOARD_UID {0x1B, 0x18, 0x7D, 0xF2, 0x21, 0xF6, 0x29, 0x48}

#define POINTING_DEVICE_HIRES_SCROLL_ENABLE 1
#define POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER 120
#define WHEEL_EXTENDED_REPORT 1

#define AXIS_TYPE int16_t

//#define FORTY_FOUR_MM_TB
/* key matrix size */
// Rows are doubled-up
#define MATRIX_ROWS  10
#define MATRIX_COLS  6
#define PFET_ROWS
#define EE_HANDS
//#define DEBUG_MATRIX_SCAN_RATE
// Svalboard stores its config in VIA custom config area
// saved_values_t (~54 bytes) + 3 bytes magic = ~60 bytes, round to 100
#define VIA_EEPROM_CUSTOM_CONFIG_SIZE 100


#define FLASH_LEN (16 * 1024 * 1024)
#define WEAR_LEVELING_BACKING_SIZE (128 * 1024)
// wiring of each half
//Layout for svalboard v0 (different from lalboard_v2)
//1 2 3 4 5 6
//S E D N W None
//Both Thumbs (these are same as lalboard_v2)
//OL OU D IL MODE DOUBLE
//Knuckle Nail Down Pad Up Double
//#define THUMB_DOWN_ACTIVE_DARK
#define MATRIX_COL_PUSHED_STATES { 0, 0, 1, 0, 0, 0 }
#ifdef THUMB_DOWN_ACTIVE_DARK
    #define MATRIX_COL_PUSHED_STATES_THUMBS { 0, 0, 1, 0, 0, 0 }
#else
    #define MATRIX_COL_PUSHED_STATES_THUMBS { 0, 0, 0, 0, 0, 0 }
#endif
#define DOUBLEDOWN_COL 5 // need a pullup on COL6

#define SERIAL_DEBUG
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET
#define RP2040_BOOTLOADER_DOUBLE_TAP_RESET_TIMEOUT 500 // Timeout window in ms in which the double tap can occur.

// Viable dynamic feature entry counts
#define VIABLE_TAP_DANCE_ENTRIES 50
#define VIABLE_COMBO_ENTRIES 50
#define VIABLE_KEY_OVERRIDE_ENTRIES 30
#define DYNAMIC_KEYMAP_MACRO_COUNT 50

// Viable defaults for Svalboard - sane mod-tap experience
#define VIABLE_DEFAULT_NKRO 1
#define VIABLE_DEFAULT_PERMISSIVE_HOLD 1
#define VIABLE_DEFAULT_CHORDAL_HOLD 1

#define USB_MAX_POWER_CONSUMPTION 500
#define USB_SUSPEND_WAKEUP_DELAY 500
#define SERIAL_USART_SPEED 1000000

#define MOUSE_EXTENDED_REPORT
#define SPLIT_POINTING_ENABLE
#define POINTING_DEVICE_COMBINED
#define POINTING_DEVICE_AUTO_MOUSE_MH_ENABLE
#define POINTING_DEVICE_TASK_THROTTLE_MS 1


// Avoid slave-slave deadlock due to missing USB_VBUS_PIN.
//
// End result of enabling this: when you plug the keyboard to a finnicky USB
// hub, KVM, or a machine that boots slowly (ECC RAM), the keyboard no longer
// needs to be reset to come to life.
#define SPLIT_WATCHDOG_ENABLE

// WS2812-fu here:
//  pretty lights
//  https://docs.qmk.fm/#/feature_rgblight?id=configuration
#define WS2812_DI_PIN GP19
#define RGBLED_SPLIT { 1, 1 }
#define RGBLIGHT_LAYERS_RETAIN_VAL
#define RGBLIGHT_LAYERS DYNAMIC_KEYMAP_LAYER_COUNT
#define RGBLIGHT_DEFAULT_SAT 0 // white?
#define RGBLIGHT_LIMIT_VAL 255
#define RGBLIGHT_DEFAULT_VAL 128
#define RGBLIGHT_SLEEP // don't annoy when host asleep
#define RGBLIGHT_MAX_LAYERS 16 //DYNAMIC_KEYMAP_LAYER_COUNT
#define RGBLIGHT_VAL_STEP 10
#define RGBLIGHT_LED_COUNT 2

#define SPLIT_TRANSACTION_IDS_KB KEYBOARD_SYNC_A

#define PERMISSIVE_HOLD

#define MOUSEKEY_WHEEL_DELTA 120

#define OS_DETECTION_KEYBOARD_RESET

#define PMW33XX_LIFTOFF_DISTANCE 0x00
