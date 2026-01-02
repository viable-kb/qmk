POINTING_DEVICE_ENABLE = yes
# Use custom wrapper that calls azoteq functions directly
POINTING_DEVICE_DRIVER = custom

# Include Azoteq driver sources for our custom wrapper to call
SRC += drivers/sensors/azoteq_iqs5xx.c
QUANTUM_LIB_SRC += i2c_master.c

EXTRA_SRC = azoteq/azoteq.c

MH_AUTO_BUTTONS = yes