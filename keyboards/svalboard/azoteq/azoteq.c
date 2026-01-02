// Azoteq IQS5xx custom driver wrapper
// Calls upstream driver functions directly - no pointing_device.c patch needed

#include "quantum.h"
#include "pointing_device.h"
#include "drivers/sensors/azoteq_iqs5xx.h"
#include "svalboard.h"

// Azoteq can only report at 100Hz. Faster polling causes issues.
#define AZO_MS 10

static uint16_t azo_timer = 0;
static uint8_t azo_held_buttons = 0;
static uint16_t azo_cached_cpi = 0;

bool pointing_device_driver_init(void) {
    azoteq_iqs5xx_init();
    return true;
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    if (timer_elapsed(azo_timer) >= AZO_MS) {
        azo_timer = timer_read();
        mouse_report = azoteq_iqs5xx_get_report(mouse_report);
        azo_held_buttons = mouse_report.buttons;
    }

    // Invert V-axis
    mouse_report.v = -mouse_report.v;
    mouse_report.buttons = azo_held_buttons;

    return mouse_report;
}

uint16_t pointing_device_driver_get_cpi(void) {
    return azo_cached_cpi;
}

void pointing_device_driver_set_cpi(uint16_t cpi) {
    if (cpi != azo_cached_cpi) {
        azoteq_iqs5xx_set_cpi(cpi);
        azo_cached_cpi = cpi;
    }
}
