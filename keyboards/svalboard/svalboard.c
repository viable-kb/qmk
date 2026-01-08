#include "svalboard.h"
#if VIA_ENABLE
#include "via.h"
#endif
#include "version.h"
#include "split_common/transactions.h"
#include <string.h>
#include QMK_KEYBOARD_H
#include "usb_main.h"
#include "suspend.h"

// USB remote wakeup status bit (from USB spec, not exported by QMK headers)
#ifndef USB_GETSTATUS_REMOTE_WAKEUP_ENABLED
#define USB_GETSTATUS_REMOTE_WAKEUP_ENABLED (2U)
#endif

saved_values_t global_saved_values;
const int16_t mh_timer_choices[6] = { 200, 300, 400, 500, 800, -1 }; // -1 is infinite.

uint8_t sval_active_layer = 0;

// Store svalboard data in VIA custom config at offset 0
#define SVALBOARD_VIA_CONFIG_OFFSET 0
#define SVALBOARD_VIA_CONFIG_SIZE sizeof(saved_values_t)

// Magic bytes for EEPROM validation - derived from QMK_BUILDDATE
// QMK_BUILDDATE format: "2019-11-05-11:29:54"
// Use full timestamp so every build gets unique magic
#define SVALBOARD_MAGIC_SIZE 6
#define SVALBOARD_MAGIC_OFFSET (SVALBOARD_VIA_CONFIG_OFFSET + SVALBOARD_VIA_CONFIG_SIZE)

#if VIA_ENABLE
static void svalboard_get_magic(uint8_t *magic) {
    char *p = QMK_BUILDDATE;
    magic[0] = ((p[2] & 0x0F) << 4) | (p[3] & 0x0F);  // year low 2 digits
    magic[1] = ((p[5] & 0x0F) << 4) | (p[6] & 0x0F);  // month
    magic[2] = ((p[8] & 0x0F) << 4) | (p[9] & 0x0F);  // day
    magic[3] = ((p[11] & 0x0F) << 4) | (p[12] & 0x0F); // hour
    magic[4] = ((p[14] & 0x0F) << 4) | (p[15] & 0x0F); // minute
    magic[5] = ((p[17] & 0x0F) << 4) | (p[18] & 0x0F); // second
}

static bool svalboard_eeprom_is_valid(void) {
    uint8_t stored[SVALBOARD_MAGIC_SIZE];
    uint8_t expected[SVALBOARD_MAGIC_SIZE];
    via_read_custom_config(stored, SVALBOARD_MAGIC_OFFSET, SVALBOARD_MAGIC_SIZE);
    svalboard_get_magic(expected);
    return memcmp(stored, expected, SVALBOARD_MAGIC_SIZE) == 0;
}

static void svalboard_eeprom_set_valid(void) {
    uint8_t magic[SVALBOARD_MAGIC_SIZE];
    svalboard_get_magic(magic);
    via_update_custom_config(magic, SVALBOARD_MAGIC_OFFSET, SVALBOARD_MAGIC_SIZE);
}

void write_eeprom_kb(void) {
    via_update_custom_config(&global_saved_values, SVALBOARD_VIA_CONFIG_OFFSET, SVALBOARD_VIA_CONFIG_SIZE);
}
#else
static bool svalboard_eeprom_is_valid(void) { return true; }
static void svalboard_eeprom_set_valid(void) {}
void write_eeprom_kb(void) {}
#endif

#define HSV(c) (struct layer_hsv) { (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF}

void read_eeprom_kb(void) {
    // Check if EEPROM data is valid (matches current firmware version)
    if (!svalboard_eeprom_is_valid()) {
        // Fresh EEPROM - apply defaults
        memset(&global_saved_values, 0, sizeof(global_saved_values));

        global_saved_values.right_dpi_index = 3;
        global_saved_values.left_dpi_index = 3;
        global_saved_values.mh_timer_index = 3;
        global_saved_values.left_scroll = true;
        global_saved_values.auto_mouse = true;
        global_saved_values.axis_scroll_lock = true;
        global_saved_values.turbo_scan = 0;
        global_saved_values.natural_scroll = false;
        global_saved_values.automouse_threshold = 50;
        global_saved_values.automouse_decay = 7;  // 70ms

        // Layer colors
        global_saved_values.layer_colors[0] = HSV(0x55FFFF);  // Green
        global_saved_values.layer_colors[1] = HSV(0x15FFFF);  // Orange
        global_saved_values.layer_colors[2] = HSV(0x95FFFF);  // Azure
        global_saved_values.layer_colors[3] = HSV(0x0BB0FF);  // Coral
        global_saved_values.layer_colors[4] = HSV(0x2BFFFF);  // Yellow
        global_saved_values.layer_colors[5] = HSV(0x80FF80);  // Teal
        global_saved_values.layer_colors[6] = HSV(0x00FFFF);  // Red
        global_saved_values.layer_colors[7] = HSV(0x00FFFF);  // Red
        global_saved_values.layer_colors[8] = HSV(0xEAFFFF);  // Pink
        global_saved_values.layer_colors[9] = HSV(0xBFFF80);  // Purple
        global_saved_values.layer_colors[10] = HSV(0x0BB0FF); // Coral
        global_saved_values.layer_colors[11] = HSV(0x6AFFFF); // Spring Green
        global_saved_values.layer_colors[12] = HSV(0x80FF80); // Teal
        global_saved_values.layer_colors[13] = HSV(0x80FFFF); // Turquoise
        global_saved_values.layer_colors[14] = HSV(0x2BFFFF); // Yellow
        global_saved_values.layer_colors[15] = HSV(0xD5FFFF); // Magenta

        write_eeprom_kb();
        svalboard_eeprom_set_valid();
    } else {
#if VIA_ENABLE
        via_read_custom_config(&global_saved_values, SVALBOARD_VIA_CONFIG_OFFSET, SVALBOARD_VIA_CONFIG_SIZE);
#endif
    }
    sval_active_layer = 0;
}

static const char YES[] = "yes";
static const char NO[] = "no";

const char *yes_or_no(int flag) {
    if (flag) {
	return YES;
    } else {
	return NO;
    }
}

const uint16_t dpi_choices[] = { 200, 400, 600, 800, 1200, 1600, 2400, 3200, 4800, 6400, 12000 }; // If we need more, add them.
#define DPI_CHOICES_LENGTH (sizeof(dpi_choices)/sizeof(dpi_choices[0]))
extern bool is_mac;

void output_keyboard_info(void) {
    char output_buffer[256];

    sprintf(output_buffer, "%s:%s @ %s\n", QMK_KEYBOARD, QMK_KEYMAP, QMK_VERSION);
    send_string(output_buffer);
    sprintf(output_buffer, "Left Ptr: Scroll %s, cpi: %d, Right Ptr: Scroll %s, cpi: %d\n",
	    yes_or_no(global_saved_values.left_scroll), dpi_choices[global_saved_values.left_dpi_index],
	    yes_or_no(global_saved_values.right_scroll), dpi_choices[global_saved_values.right_dpi_index]);
    send_string(output_buffer);
    sprintf(output_buffer, "Axis Scroll Lock: %s (is Mac: %d), Natural Scroll: %s, Mouse Layer: %s, Mouse Layer Timeout: %d, Turbo Scan: %d\n",
	    yes_or_no(global_saved_values.axis_scroll_lock),
	    is_mac,
	    yes_or_no(global_saved_values.natural_scroll),
	    yes_or_no(global_saved_values.auto_mouse),
	    mh_timer_choices[global_saved_values.mh_timer_index],
	    global_saved_values.turbo_scan);
    send_string(output_buffer);
}

const uint16_t sval_postwait_us[] = {90, 60, 45, 30, 25, 20, 15};
const uint16_t sval_prewait_us[] = {90, 60, 45, 30, 25, 20, 15};
#define TURBO_CHOICES_LENGTH (sizeof(sval_postwait_us)/sizeof(sval_postwait_us[0]))
void change_turbo_scan(void) {
    if (global_saved_values.turbo_scan + 1 < TURBO_CHOICES_LENGTH) {
        global_saved_values.turbo_scan++;
    } else {
        global_saved_values.turbo_scan = 0;
    }
    write_eeprom_kb();
}

void increase_left_dpi(void) {
    if (global_saved_values.left_dpi_index + 1 < DPI_CHOICES_LENGTH) {
        global_saved_values.left_dpi_index++;
        set_left_dpi(global_saved_values.left_dpi_index);
        write_eeprom_kb();
    }
}

void decrease_left_dpi(void) {
    if (global_saved_values.left_dpi_index > 0) {
        global_saved_values.left_dpi_index--;
        set_left_dpi(global_saved_values.left_dpi_index);
        write_eeprom_kb();
    }
}

void increase_right_dpi(void) {
    if (global_saved_values.right_dpi_index + 1 < DPI_CHOICES_LENGTH) {
        global_saved_values.right_dpi_index++;
        set_right_dpi(global_saved_values.right_dpi_index);
        write_eeprom_kb();
    }
}

void decrease_right_dpi(void) {
    if (global_saved_values.right_dpi_index > 0) {
        global_saved_values.right_dpi_index--;
        set_right_dpi(global_saved_values.right_dpi_index);
        write_eeprom_kb();
    }
}

int16_t get_left_dpi() {
    return dpi_choices[global_saved_values.left_dpi_index];
}

int16_t get_right_dpi() {
    return dpi_choices[global_saved_values.right_dpi_index];
}

void set_left_dpi(uint8_t index) {
    uprintf("LDPI: %d %d\n", index, dpi_choices[index]);
    pointing_device_set_cpi_on_side(true, dpi_choices[index]);
}

void set_right_dpi(uint8_t index) {
    uprintf("RDPI: %d %d\n", index, dpi_choices[index]);
    pointing_device_set_cpi_on_side(false, dpi_choices[index]);
}

void set_dpi_from_eeprom(void) {
    set_left_dpi(global_saved_values.left_dpi_index);
    set_right_dpi(global_saved_values.right_dpi_index);
}

void sval_set_active_layer(uint32_t layer, bool save) {
    if (layer > 15) layer = 15;
    sval_active_layer = layer;
    struct layer_hsv cols = global_saved_values.layer_colors[layer];
    if (save) {
        rgblight_sethsv(cols.hue, cols.sat, rgblight_get_val()); //store using current brightness
    } else {
        rgblight_sethsv_noeeprom(cols.hue, cols.sat, rgblight_get_val()); //reuse currrent brightness
    }
}

// RPC listener for split keyboard sync
void kb_sync_listener(uint8_t in_buflen, const void* in_data, uint8_t out_buflen, void* out_data) {
    global_saved_values.turbo_scan = ((const presence_rpc_t *)in_data)->turbo_scan;
}

void keyboard_post_init_kb(void) {
    read_eeprom_kb();
    set_dpi_from_eeprom();
    keyboard_post_init_user();
    transaction_register_rpc(KEYBOARD_SYNC_A, kb_sync_listener);
    if (is_keyboard_master()) {
        sval_set_active_layer(sval_active_layer, false);
    }
}

static bool is_connected = false;

// Custom USB wake handler for split keyboards with NO_USB_STARTUP_CHECK
// This replaces the wake functionality that NO_USB_STARTUP_CHECK disables
static void sval_usb_wake_handler(void) {
    // Only master half handles USB wake
    if (!is_keyboard_master()) return;

    // Check if USB is in suspended state
    if (USB_DRIVER.state == USB_SUSPENDED) {
        // Check if host has enabled remote wakeup capability
        if (USB_DRIVER.status & USB_GETSTATUS_REMOTE_WAKEUP_ENABLED) {
            // Check for wake condition (key pressed)
            if (suspend_wakeup_condition()) {
                usbWakeupHost(&USB_DRIVER);
#if USB_SUSPEND_WAKEUP_DELAY > 0
                wait_ms(USB_SUSPEND_WAKEUP_DELAY);
#endif
            }
        }
    }
}

void housekeeping_task_kb(void) {
    sval_usb_wake_handler();

    if (is_keyboard_master()) {
        static uint32_t last_ping = 0;
        if (timer_elapsed(last_ping) > 500) {
            presence_rpc_t rpcout = {global_saved_values.turbo_scan};
            presence_rpc_t rpcin = {0};
            if (transaction_rpc_exec(KEYBOARD_SYNC_A, sizeof(presence_rpc_t), &rpcout, sizeof(presence_rpc_t), &rpcin)) {
                if (!is_connected) {
                    is_connected = true;
                    sval_on_reconnect();
                }
            } else {
                is_connected = false;
            }
            last_ping = timer_read32();
        }
    }
}

void sval_on_reconnect(void) {
    // Reset colors, or it won't communicate the right color.
    rgblight_sethsv_noeeprom(0, 0, rgblight_get_val()); //reuse existing (eeprom) val, so brightness doesn't reset
    sval_set_active_layer(sval_active_layer, true);
}

#ifndef SVALBOARD_REENABLE_BOOTMAGIC_LITE
// This is to override `bootmagic_lite` feature (see docs/feature_bootmagic.md),
// which can't be turned off in the usual way (via info.json) because setting
// `VIA_ENABLE` forces `BOOTMAGIC_ENABLE` in `builddefs/common_features.mk`.
//
// Obviously if you find this feature useful, you might want to define the
// SVALBOARD_... gating macro, and then possibly also (re-)define the
// `"bootmagic": { "matrix": [X,Y] },` in `info.json` to point the matrix at
// a more useful key than the [0,0] default. Ideally a center key, which is
// normally ~always present. Because the default (thumb knuckle) means that
// if you boot with the key pulled out, the eeprom gets cleared.

void bootmagic_lite(void) {
  // boo!
}
#endif

__attribute__((weak)) void recalibrate_pointer(void) {
}


const char chordal_hold_layout[MATRIX_ROWS][MATRIX_COLS] PROGMEM =
    LAYOUT(
            'R', 'R', 'R', 'R', 'R', 'R',
            'R', 'R', 'R', 'R', 'R', 'R',
            'R', 'R', 'R', 'R', 'R', 'R',
            'R', 'R', 'R', 'R', 'R', 'R',
            'L', 'L', 'L', 'L', 'L', 'L',
            'L', 'L', 'L', 'L', 'L', 'L',
            'L', 'L', 'L', 'L', 'L', 'L',
            'L', 'L', 'L', 'L', 'L', 'L',
            '*', '*', '*', '*', '*', '*',
            '*', '*', '*', '*', '*', '*'
          );

#if VIA_ENABLE
// VIA custom keyboard value IDs (channel 1)
enum sval_via_value_id {
    id_left_dpi = 0,
    id_left_scroll = 1,
    id_right_dpi = 2,
    id_right_scroll = 3,
    id_automouse_enable = 4,
    id_automouse_timeout = 5,
    id_automouse_threshold = 6,
    id_natural_scroll = 7,
    id_axis_lock = 8,
    id_turbo_scan = 9,
    id_automouse_decay = 10,  // Accumulator decay time in 10ms units
    id_tapping_term = 16,
    id_permissive_hold = 17,
    id_hold_on_other_key = 18,
    id_retro_tapping = 19,
    // 20-31 reserved
    id_layer0_color = 32,
    // 32-47 are layer colors (id_layer0_color + layer)
};

void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    // New VIA API: data[0]=command, data[1]=channel, data[2]=value_id, data[3+]=value_data
    uint8_t command = data[0];
    uint8_t *value_id = &data[2];
    uint8_t *value_data = &data[3];

    switch (command) {
        case id_custom_set_value:
            switch (*value_id) {
                case id_left_dpi:
                    if (value_data[0] < DPI_CHOICES_LENGTH) {
                        global_saved_values.left_dpi_index = value_data[0];
                        set_left_dpi(value_data[0]);
                    }
                    break;
                case id_left_scroll:
                    global_saved_values.left_scroll = value_data[0];
                    break;
                case id_right_dpi:
                    if (value_data[0] < DPI_CHOICES_LENGTH) {
                        global_saved_values.right_dpi_index = value_data[0];
                        set_right_dpi(value_data[0]);
                    }
                    break;
                case id_right_scroll:
                    global_saved_values.right_scroll = value_data[0];
                    break;
                case id_automouse_enable:
                    global_saved_values.auto_mouse = value_data[0];
                    break;
                case id_automouse_timeout:
                    if (value_data[0] < 6) {
                        global_saved_values.mh_timer_index = value_data[0];
                    }
                    break;
                case id_natural_scroll:
                    global_saved_values.natural_scroll = value_data[0];
                    break;
                case id_axis_lock:
                    global_saved_values.axis_scroll_lock = value_data[0];
                    break;
                case id_turbo_scan:
                    if (value_data[0] < TURBO_CHOICES_LENGTH) {
                        global_saved_values.turbo_scan = value_data[0];
                    }
                    break;
                case id_automouse_threshold:
                    global_saved_values.automouse_threshold = value_data[0] | (value_data[1] << 8);
                    break;
                case id_automouse_decay:
                    global_saved_values.automouse_decay = value_data[0];
                    break;
                default:
                    // Layer colors: id 32-47
                    if (*value_id >= id_layer0_color && *value_id < id_layer0_color + 16) {
                        uint8_t layer = *value_id - id_layer0_color;
                        // VIA color is H, S (2 bytes)
                        global_saved_values.layer_colors[layer].hue = value_data[0];
                        global_saved_values.layer_colors[layer].sat = value_data[1];
                        // Update display if this is the active layer
                        if (layer == sval_active_layer) {
                            sval_set_active_layer(layer, false);
                        }
                    }
                    break;
            }
            break;

        case id_custom_get_value:
            switch (*value_id) {
                case id_left_dpi:
                    value_data[0] = global_saved_values.left_dpi_index;
                    break;
                case id_left_scroll:
                    value_data[0] = global_saved_values.left_scroll;
                    break;
                case id_right_dpi:
                    value_data[0] = global_saved_values.right_dpi_index;
                    break;
                case id_right_scroll:
                    value_data[0] = global_saved_values.right_scroll;
                    break;
                case id_automouse_enable:
                    value_data[0] = global_saved_values.auto_mouse;
                    break;
                case id_automouse_timeout:
                    value_data[0] = global_saved_values.mh_timer_index;
                    break;
                case id_natural_scroll:
                    value_data[0] = global_saved_values.natural_scroll;
                    break;
                case id_axis_lock:
                    value_data[0] = global_saved_values.axis_scroll_lock;
                    break;
                case id_turbo_scan:
                    value_data[0] = global_saved_values.turbo_scan;
                    break;
                case id_automouse_threshold:
                    value_data[0] = global_saved_values.automouse_threshold & 0xFF;
                    value_data[1] = (global_saved_values.automouse_threshold >> 8) & 0xFF;
                    break;
                case id_automouse_decay:
                    value_data[0] = global_saved_values.automouse_decay;
                    break;
                default:
                    // Layer colors: id 32-47
                    if (*value_id >= id_layer0_color && *value_id < id_layer0_color + 16) {
                        uint8_t layer = *value_id - id_layer0_color;
                        value_data[0] = global_saved_values.layer_colors[layer].hue;
                        value_data[1] = global_saved_values.layer_colors[layer].sat;
                    }
                    break;
            }
            break;

        case id_custom_save:
            write_eeprom_kb();
            break;
    }
}
#endif // VIA_ENABLE
