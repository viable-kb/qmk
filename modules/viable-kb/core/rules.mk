# Viable - VIA3-based dynamic features module
# Provides: tap dance, combo, key override, leader, keyboard definition

OPT_DEFS += -DVIABLE_ENABLE

# Set USB serial number for GUI detection (unless keyboard overrides it)
SERIAL_NUMBER ?= viable:12345-00
OPT_DEFS += -DSERIAL_NUMBER=\"$(SERIAL_NUMBER)\"

# Use different raw HID usage page/id than Vial (0xFF60/0x61) to avoid enumeration conflicts
OPT_DEFS += -DRAW_USAGE_PAGE=0xFF61
OPT_DEFS += -DRAW_USAGE_ID=0x62

# Enable tapping term per key for tap dance custom timing
TAPPING_TERM_PER_KEY ?= yes

# Module path for source files
VIABLE_MODULE_PATH := $(dir $(lastword $(MAKEFILE_LIST)))

# Search for viable.json in keymap directories (same search order as QMK)
VIABLE_JSON_PATH := $(firstword \
    $(wildcard $(MAIN_KEYMAP_PATH_1)/viable.json) \
    $(wildcard $(MAIN_KEYMAP_PATH_2)/viable.json) \
    $(wildcard $(MAIN_KEYMAP_PATH_3)/viable.json) \
    $(wildcard $(MAIN_KEYMAP_PATH_4)/viable.json) \
    $(wildcard $(MAIN_KEYMAP_PATH_5)/viable.json))

ifeq ($(VIABLE_JSON_PATH),)
    $(error Viable module requires viable.json in your keymap directory)
endif

# Generated file paths
VIABLE_CONFIG_HEADER := $(INTERMEDIATE_OUTPUT)/src/viable_config.h
VIABLE_CONFIG_MK := $(INTERMEDIATE_OUTPUT)/src/viable_config.mk
VIABLE_DEFINITION_HEADER := $(INTERMEDIATE_OUTPUT)/src/viable_definition_data.h

# Generate config header and make fragment from viable.json
# This determines which features are enabled based on the 'viable' object in JSON
$(shell mkdir -p "$(INTERMEDIATE_OUTPUT)/src" && \
    python3 "$(VIABLE_MODULE_PATH)viable_config.py" \
        "$(VIABLE_JSON_PATH)" \
        "$(VIABLE_CONFIG_HEADER)" \
        "$(VIABLE_CONFIG_MK)" >/dev/null 2>&1)

# Include generated make fragment (sets TAP_DANCE_ENABLE, COMBO_ENABLE, etc.)
-include $(VIABLE_CONFIG_MK)

# Explicitly add OPT_DEFS for features (in case module is processed after generic_features.mk)
ifeq ($(LEADER_ENABLE),yes)
    OPT_DEFS += -DLEADER_ENABLE
endif

# Add include path for generated headers
VPATH += $(INTERMEDIATE_OUTPUT)/src

# Generate compressed keyboard definition header
$(shell python3 "$(VIABLE_MODULE_PATH)viable_compress.py" \
    "$(VIABLE_JSON_PATH)" \
    "$(VIABLE_DEFINITION_HEADER)" >/dev/null 2>&1)

# Core source files (always included)
SRC += $(VIABLE_MODULE_PATH)viable.c
SRC += $(VIABLE_MODULE_PATH)viable_definition.c
SRC += $(VIABLE_MODULE_PATH)viable_qmk_settings.c
SRC += $(VIABLE_MODULE_PATH)viable_fragments.c
SRC += $(VIABLE_MODULE_PATH)client_wrapper.c

# Conditionally include feature source files based on viable.json config
ifeq ($(TAP_DANCE_ENABLE),yes)
    SRC += $(VIABLE_MODULE_PATH)viable_tap_dance.c
endif

ifeq ($(COMBO_ENABLE),yes)
    SRC += $(VIABLE_MODULE_PATH)viable_combo.c
endif

ifeq ($(KEY_OVERRIDE_ENABLE),yes)
    SRC += $(VIABLE_MODULE_PATH)viable_key_override.c
endif

ifeq ($(LEADER_ENABLE),yes)
    SRC += $(VIABLE_MODULE_PATH)viable_leader.c
endif

# Alt repeat key doesn't have a QMK enable flag, check for entries directly
ifneq ($(wildcard $(VIABLE_CONFIG_HEADER)),)
    ifneq ($(shell grep -c VIABLE_ALT_REPEAT_KEY_ENABLE $(VIABLE_CONFIG_HEADER)),0)
        SRC += $(VIABLE_MODULE_PATH)viable_alt_repeat_key.c
    endif
endif
