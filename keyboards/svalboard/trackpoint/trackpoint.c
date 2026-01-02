// TrackPoint (PS/2) - Sprintek-specific configuration
// Uses svalboard/pointing_device_ps2 module for PS/2 integration

#include "quantum.h"
#include "pointing_device.h"
#include "pointing_device_ps2.h"
#include "ps2.h"

// Override get_report to apply axis rotation for svalboard orientation
report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    mouse_report = ps2_mouse_get_report(mouse_report);

    // 90° rotation: x=y, y=-x
    int16_t swap = mouse_report.x;
    mouse_report.x = mouse_report.y;
    mouse_report.y = -swap;

    return mouse_report;
}

void pointing_device_init_kb(void) {
    // TrackPoint-specific initialization
    // Disable Z-tap
    ps2_mouse_disable_data_reporting();

    PS2_MOUSE_SEND(0xE2, "pts: 0xE2");
    PS2_MOUSE_SEND(0x47, "pts: 0x47");
    PS2_MOUSE_SEND(0x2C, "pts: 0x2C");
    PS2_MOUSE_SEND(0x00, "pts: 0x00");  // 01 turns on Z tap, 00 turns off
    PS2_MOUSE_RECEIVE("ztap - disable");

    // Disable drift correction (Sprintek command)
    uint8_t val;
    PS2_MOUSE_SEND(0xE2, "pts: 0xE2");
    PS2_MOUSE_SEND(0x80, "pts: 0x80");
    PS2_MOUSE_SEND(0xFA, "pts: 0xFA");
    val = ps2_host_recv_response();

    if (~val & 0x10) {
        PS2_MOUSE_SEND(0xE2, "pts: 0xE2");
        PS2_MOUSE_SEND(0x47, "pts: 0x47");
        PS2_MOUSE_SEND(0xFA, "pts: 0xFA");
        PS2_MOUSE_SEND(0x10, "pts: 0x10");
        PS2_MOUSE_RECEIVE("drift correction - disable");
    }

    // Set deadzone (increase to reduce tail drift)
    PS2_MOUSE_SEND(0xE2, "pts: 0xE2");
    PS2_MOUSE_SEND(0x81, "pts: 0x81");
    PS2_MOUSE_SEND(0x92, "pts: 0x92");
    PS2_MOUSE_SEND(0x15, "pts: 0x15");  // Experiment with this value if needed

    // Disable double Z tap
    PS2_MOUSE_SEND(0xE2, "pts: 0xE2");
    PS2_MOUSE_SEND(0x81, "pts: 0x81");
    PS2_MOUSE_SEND(0x5E, "pts: 0x5E");
    PS2_MOUSE_SEND(0x00, "pts: 0x00");
    PS2_MOUSE_RECEIVE("double Z tap - disable");

    ps2_mouse_enable_data_reporting();

    pointing_device_init_user();
}

void recalibrate_pointer(void) {
    PS2_MOUSE_SEND(0xE2, "pts: 0xE2");
    PS2_MOUSE_SEND(0x51, "pts: 0x51");
    PS2_MOUSE_RECEIVE("trackpoint - recalibrate");
}
