// Copyright 2025 Ira Cooper <ira@wakeful.net>
// SPDX-License-Identifier: GPL-2.0-or-later

/**
 * Fragment hardware detection and EEPROM selection storage.
 *
 * Fragments are modular physical keyboard components (finger modules, thumb clusters)
 * that can be detected via hardware pins or GPIO and selected by users.
 *
 * EEPROM Layout:
 * Fragment selections are stored after the leader entries in EEPROM.
 * Fixed 21-byte buffer (VIABLE_FRAGMENT_MAX_INSTANCES bytes).
 */

#include "viable.h"
#include "eeconfig.h"
#include <string.h>

// Include generated config (contains VIABLE_FRAGMENT_INSTANCE_COUNT)
#if __has_include("viable_config.h")
#    include "viable_config.h"
#endif

// Default instance count if not provided by config
#ifndef VIABLE_FRAGMENT_INSTANCE_COUNT
#    define VIABLE_FRAGMENT_INSTANCE_COUNT 0
#endif

// EEPROM offset for fragment selections (uses VIABLE_FRAGMENT_OFFSET from viable.h)

// Internal EEPROM access (same as viable.c)
static void fragment_read_eeprom(uint16_t offset, void *buf, uint16_t size) {
    eeconfig_read_kb_datablock(buf, offset, size);
}

static void fragment_write_eeprom(uint16_t offset, const void *buf, uint16_t size) {
    eeconfig_update_kb_datablock(buf, offset, size);
}

// Weak function for hardware detection - keyboard should override
__attribute__((weak)) uint8_t viable_fragment_detect(uint8_t instance_idx) {
    (void)instance_idx;
    return VIABLE_FRAGMENT_ID_NONE;  // No detection by default
}

// Get instance count from build-time config
uint8_t viable_fragment_get_instance_count(void) {
    return VIABLE_FRAGMENT_INSTANCE_COUNT;
}

// Get EEPROM-stored selection for an instance
uint8_t viable_fragment_get_selection(uint8_t instance_idx) {
    if (instance_idx >= VIABLE_FRAGMENT_MAX_INSTANCES) {
        return VIABLE_FRAGMENT_ID_NONE;
    }

    uint8_t selection;
    fragment_read_eeprom(VIABLE_FRAGMENT_OFFSET + instance_idx, &selection, 1);
    return selection;
}

// Set EEPROM-stored selection for an instance
void viable_fragment_set_selection(uint8_t instance_idx, uint8_t fragment_id) {
    if (instance_idx >= VIABLE_FRAGMENT_MAX_INSTANCES) {
        return;
    }

    fragment_write_eeprom(VIABLE_FRAGMENT_OFFSET + instance_idx, &fragment_id, 1);
}

// Protocol handler: Get hardware detection results
// Request:  [0xDF] [0x18]
// Response: [0xDF] [0x18] [count] [frag0] [frag1] ... [frag20] (fixed 21-byte buffer)
bool viable_handle_fragment_get_hardware(uint8_t *data, uint8_t length) {
    (void)length;

    uint8_t count = viable_fragment_get_instance_count();
    data[2] = count;

    // Fill buffer with hardware detection results
    for (uint8_t i = 0; i < VIABLE_FRAGMENT_MAX_INSTANCES; i++) {
        if (i < count) {
            data[3 + i] = viable_fragment_detect(i);
        } else {
            data[3 + i] = VIABLE_FRAGMENT_ID_NONE;  // Unused slots
        }
    }

    return true;
}

// Protocol handler: Get EEPROM selections
// Request:  [0xDF] [0x19]
// Response: [0xDF] [0x19] [count] [frag0] [frag1] ... [frag20] (fixed 21-byte buffer)
bool viable_handle_fragment_get_selections(uint8_t *data, uint8_t length) {
    (void)length;

    uint8_t count = viable_fragment_get_instance_count();
    data[2] = count;

    // Read all selections from EEPROM
    for (uint8_t i = 0; i < VIABLE_FRAGMENT_MAX_INSTANCES; i++) {
        if (i < count) {
            data[3 + i] = viable_fragment_get_selection(i);
        } else {
            data[3 + i] = VIABLE_FRAGMENT_ID_NONE;  // Unused slots
        }
    }

    return true;
}

// Protocol handler: Set EEPROM selections
// Request:  [0xDF] [0x1A] [count] [frag0] [frag1] ... [frag20] (fixed 21-byte buffer)
// Response: [0xDF] [0x1A] [status] (0x00 = success)
bool viable_handle_fragment_set_selections(uint8_t *data, uint8_t length) {
    if (length < 24) {  // 0xDF + 0x1A + count + 21 bytes
        data[2] = 0x01;  // Error: invalid length
        return true;
    }

    uint8_t count = data[2];
    if (count > VIABLE_FRAGMENT_MAX_INSTANCES) {
        data[2] = 0x02;  // Error: count too large
        return true;
    }

    // Write all selections to EEPROM
    for (uint8_t i = 0; i < count; i++) {
        viable_fragment_set_selection(i, data[3 + i]);
    }

    data[2] = 0x00;  // Success
    return true;
}
