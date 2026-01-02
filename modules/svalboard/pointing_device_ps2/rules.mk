# PS/2 Pointing Device Module
# Provides pointing_device integration for PS/2 mice and trackpoints

# Use custom driver - module provides pointing_device_driver_* functions
POINTING_DEVICE_DRIVER = custom

# Enable PS/2 low-level protocol (ps2_vendor.c)
PS2_ENABLE = yes
PS2_DRIVER ?= vendor

# NOTE: We intentionally do NOT set PS2_MOUSE_ENABLE
# The upstream ps2_mouse.c uses the old ps2_mouse_task() model.
# This module provides pointing_device integration instead.

# Add module source and include path
POINTING_DEVICE_PS2_PATH := $(dir $(lastword $(MAKEFILE_LIST)))
SRC += $(POINTING_DEVICE_PS2_PATH)pointing_device_ps2.c
VPATH += $(POINTING_DEVICE_PS2_PATH)
