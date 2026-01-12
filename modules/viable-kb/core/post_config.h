// Copyright 2025 Ira Cooper <ira@wakeful.net>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Include generated config from viable.json (defines VIABLE_*_ENTRIES)
#include "viable_config.h"

// Defaults for features not in viable.json (0 = disabled)
#ifndef VIABLE_TAP_DANCE_ENTRIES
#    define VIABLE_TAP_DANCE_ENTRIES 0
#endif

#ifndef VIABLE_COMBO_ENTRIES
#    define VIABLE_COMBO_ENTRIES 0
#endif

#ifndef VIABLE_KEY_OVERRIDE_ENTRIES
#    define VIABLE_KEY_OVERRIDE_ENTRIES 0
#endif

#ifndef VIABLE_ALT_REPEAT_KEY_ENTRIES
#    define VIABLE_ALT_REPEAT_KEY_ENTRIES 0
#endif

#ifndef VIABLE_LEADER_ENTRIES
#    define VIABLE_LEADER_ENTRIES 0
#endif

// QMK settings storage size (viable_qmk_settings_t)
#define VIABLE_QMK_SETTINGS_SIZE 44

// Dynamic leader timeout - Viable controls this variable
#if defined(LEADER_ENABLE) && !defined(__ASSEMBLER__)
#include <stdint.h>
extern uint16_t viable_leader_timeout;
#define LEADER_TIMEOUT (viable_leader_timeout)
#endif

// Total size: tap_dance*10 + combo*12 + key_override*12 + alt_repeat*6 + one_shot(3) + leader*14 + magic(6) + qmk_settings(44) + fragments(21)
#define VIABLE_EEPROM_SIZE_CALC ( \
    (VIABLE_TAP_DANCE_ENTRIES * 10) + \
    (VIABLE_COMBO_ENTRIES * 12) + \
    (VIABLE_KEY_OVERRIDE_ENTRIES * 12) + \
    (VIABLE_ALT_REPEAT_KEY_ENTRIES * 6) + \
    3 + \
    (VIABLE_LEADER_ENTRIES * 14) + \
    6 + VIABLE_QMK_SETTINGS_SIZE + 21)

#ifndef EECONFIG_KB_DATA_SIZE
#    define EECONFIG_KB_DATA_SIZE VIABLE_EEPROM_SIZE_CALC
#endif
