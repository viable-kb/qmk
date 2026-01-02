# this turns on trackballs
POINTING_DEVICE_ENABLE = yes
# Use custom wrapper that calls pmw33xx functions directly
POINTING_DEVICE_DRIVER = custom
# Define driver type for pmw33xx_common.h to include correct header
OPT_DEFS += -DPOINTING_DEVICE_IS_PMW3389 -DPOINTING_DEVICE_DRIVER_pmw3389

# Include PMW3389 driver sources (trackball.c is auto-included by QMK)
SRC += drivers/sensors/pmw33xx_common.c drivers/sensors/pmw3389.c
QUANTUM_LIB_SRC += spi_master.c

MH_AUTO_BUTTONS = yes