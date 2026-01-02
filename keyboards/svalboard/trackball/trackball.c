// Trackball (PMW3360/PMW3389) custom driver wrapper
// Calls upstream driver functions directly - no pointing_device.c patch needed

#include "quantum.h"
#include "pointing_device.h"
#include "drivers/sensors/pmw33xx_common.h"

static uint16_t trackball_cached_cpi = 0;

bool pointing_device_driver_init(void) {
    return pmw33xx_init(0);
}

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    mouse_report = pmw33xx_get_report(mouse_report);

    // Axis swap for svalboard orientation
    int16_t swap = mouse_report.x;
    mouse_report.x = -mouse_report.y;
    mouse_report.y = -swap;

    return mouse_report;
}

uint16_t pointing_device_driver_get_cpi(void) {
    return trackball_cached_cpi;
}

void pointing_device_driver_set_cpi(uint16_t cpi) {
    if (cpi != trackball_cached_cpi) {
        pmw33xx_set_cpi_all_sensors(cpi);
        trackball_cached_cpi = cpi;
    }
}
