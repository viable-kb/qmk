# Viable - VIA3-based dynamic features module
# Provides: tap dance, combo, key override, keyboard definition

OPT_DEFS += -DVIABLE_ENABLE

# Set USB serial number for GUI detection (unless keyboard overrides it)
SERIAL_NUMBER ?= viable:12345-00
OPT_DEFS += -DSERIAL_NUMBER=\"$(SERIAL_NUMBER)\"

# Use different raw HID usage page/id than Vial (0xFF60/0x61) to avoid enumeration conflicts
OPT_DEFS += -DRAW_USAGE_PAGE=0xFF61
OPT_DEFS += -DRAW_USAGE_ID=0x62

# Entry counts are defined in keyboard config.h
# No need to set defaults here - the header has #ifndef guards

# Enable required QMK features if not already enabled
TAP_DANCE_ENABLE ?= yes
COMBO_ENABLE ?= yes
KEY_OVERRIDE_ENABLE ?= yes

# Enable tapping term per key for tap dance custom timing
TAPPING_TERM_PER_KEY ?= yes

# Add viable source files
VIABLE_MODULE_PATH := $(dir $(lastword $(MAKEFILE_LIST)))
SRC += $(VIABLE_MODULE_PATH)viable.c
SRC += $(VIABLE_MODULE_PATH)viable_tap_dance.c
SRC += $(VIABLE_MODULE_PATH)viable_combo.c
SRC += $(VIABLE_MODULE_PATH)viable_key_override.c
SRC += $(VIABLE_MODULE_PATH)viable_alt_repeat_key.c
SRC += $(VIABLE_MODULE_PATH)viable_definition.c
SRC += $(VIABLE_MODULE_PATH)viable_qmk_settings.c

# Generate compressed keyboard definition header
# Search for viable.json in keymap directories (same search order as QMK)
VIABLE_JSON_PATH := $(firstword \
    $(wildcard $(MAIN_KEYMAP_PATH_1)/viable.json) \
    $(wildcard $(MAIN_KEYMAP_PATH_2)/viable.json) \
    $(wildcard $(MAIN_KEYMAP_PATH_3)/viable.json) \
    $(wildcard $(MAIN_KEYMAP_PATH_4)/viable.json) \
    $(wildcard $(MAIN_KEYMAP_PATH_5)/viable.json))

VIABLE_DEFINITION_HEADER := $(INTERMEDIATE_OUTPUT)/src/viable_definition_data.h

# Generate the header at the start of the build
$(shell mkdir -p "$(INTERMEDIATE_OUTPUT)/src" && \
    python3 "$(VIABLE_MODULE_PATH)viable_compress.py" \
        "$(VIABLE_JSON_PATH)" \
        "$(VIABLE_DEFINITION_HEADER)" >/dev/null 2>&1)
