// Copyright 2011, 2013 Jun Wako <wakojun@gmail.com>
// Copyright 2023 Johannes H. Jensen <joh@pseudoberries.com>
// Copyright 2025 Ira Cooper <ira@wakeful.net>
// SPDX-License-Identifier: GPL-2.0-or-later

// PS/2 Pointing Device Module
// Provides pointing_device integration for PS/2 mice and trackpoints.
// Based on vial-qmk's ps2_mouse rewrite (PR #22532 approach).

#include <stdbool.h>
#include "pointing_device_ps2.h"
#include "wait.h"
#include "gpio.h"
#include "timer.h"
#include "report.h"
#include "ps2.h"
#include "pointing_device.h"

#ifdef POINTING_DEVICE_DEBUG
#    include "pointing_device_internal.h"
#endif

// Track current PS/2 mouse mode
static enum {
    PS2_MOUSE_STREAM_MODE,
    PS2_MOUSE_REMOTE_MODE,
} ps2_mouse_mode = PS2_MOUSE_STREAM_MODE;

/* ============================= PS/2 MOUSE FUNCTIONS ============================ */

void ps2_mouse_disable_data_reporting(void) {
    PS2_MOUSE_SEND(PS2_MOUSE_CMD_DISABLE_DATA_REPORTING, "ps2 mouse disable data reporting");
}

void ps2_mouse_enable_data_reporting(void) {
    PS2_MOUSE_SEND(PS2_MOUSE_CMD_ENABLE_DATA_REPORTING, "ps2 mouse enable data reporting");
}

void ps2_mouse_set_remote_mode(void) {
    PS2_MOUSE_SEND_SAFE(PS2_MOUSE_CMD_SET_REMOTE_MODE, "ps2 mouse set remote mode");
    ps2_mouse_mode = PS2_MOUSE_REMOTE_MODE;
}

void ps2_mouse_set_stream_mode(void) {
    PS2_MOUSE_SEND_SAFE(PS2_MOUSE_CMD_SET_STREAM_MODE, "ps2 mouse set stream mode");
    ps2_mouse_mode = PS2_MOUSE_STREAM_MODE;
}

void ps2_mouse_set_scaling_2_1(void) {
    PS2_MOUSE_SEND_SAFE(PS2_MOUSE_CMD_SET_SCALING_2_1, "ps2 mouse set scaling 2:1");
}

void ps2_mouse_set_scaling_1_1(void) {
    PS2_MOUSE_SEND_SAFE(PS2_MOUSE_CMD_SET_SCALING_1_1, "ps2 mouse set scaling 1:1");
}

void ps2_mouse_set_resolution(uint8_t resolution) {
    PS2_MOUSE_SET_SAFE(PS2_MOUSE_CMD_SET_RESOLUTION, resolution, "ps2 mouse set resolution");
}

void ps2_mouse_set_sample_rate(uint8_t sample_rate) {
    PS2_MOUSE_SET_SAFE(PS2_MOUSE_CMD_SET_SAMPLE_RATE, sample_rate, "ps2 mouse set sample rate");
}

static inline void ps2_mouse_enable_scrolling(void) {
    PS2_MOUSE_SEND(PS2_MOUSE_CMD_SET_SAMPLE_RATE, "Initiating scroll wheel enable: Set sample rate");
    PS2_MOUSE_SEND(200, "200");
    PS2_MOUSE_SEND(PS2_MOUSE_CMD_SET_SAMPLE_RATE, "Set sample rate");
    PS2_MOUSE_SEND(100, "100");
    PS2_MOUSE_SEND(PS2_MOUSE_CMD_SET_SAMPLE_RATE, "Set sample rate");
    PS2_MOUSE_SEND(80, "80");
    PS2_MOUSE_SEND(PS2_MOUSE_CMD_GET_DEVICE_ID, "Finished enabling scroll wheel");
    wait_ms(20);
}

/* ============================= DRIVER IMPLEMENTATION ============================ */

static bool ps2_mouse_init_core(void) {
    ps2_host_init();

    wait_ms(PS2_MOUSE_INIT_DELAY);

    PS2_MOUSE_SEND(PS2_MOUSE_CMD_RESET, "ps2_mouse_init: sending reset");

    PS2_MOUSE_RECEIVE("ps2_mouse_init: read BAT");
    PS2_MOUSE_RECEIVE("ps2_mouse_init: read DevID");

#ifdef PS2_MOUSE_USE_REMOTE_MODE
    ps2_mouse_set_remote_mode();
#else
    ps2_mouse_enable_data_reporting();
    ps2_mouse_set_stream_mode();
#endif

#ifdef PS2_MOUSE_ENABLE_SCROLLING
    ps2_mouse_enable_scrolling();
#endif

#ifdef PS2_MOUSE_USE_2_1_SCALING
    ps2_mouse_set_scaling_2_1();
#endif

    return true;
}

static inline void ps2_mouse_convert_report_to_hid(ps2_mouse_report_t *ps2_report, report_mouse_t *mouse_report) {
    bool x_sign        = ps2_report->head.b.x_sign;
    bool y_sign        = ps2_report->head.b.y_sign;
    bool left_button   = ps2_report->head.b.left_button;
    bool right_button  = ps2_report->head.b.right_button;
    bool middle_button = ps2_report->head.b.middle_button;

    // PS/2 mouse data is '9-bit integer'(-256 to 255), comprised of sign-bit and 8-bit value.
    int16_t x = x_sign ? (ps2_report->x | ~0xFF) : ps2_report->x;
    int16_t y = y_sign ? (ps2_report->y | ~0xFF) : ps2_report->y;

    x *= PS2_MOUSE_X_MULTIPLIER;
    y *= PS2_MOUSE_Y_MULTIPLIER;

    // Constrain to HID report range (-127 to 127)
    mouse_report->x = (x < -127) ? -127 : (x > 127) ? 127 : x;
    mouse_report->y = (y < -127) ? -127 : (y > 127) ? 127 : y;

    // Invert Y coordinate to conform to USB HID mouse
    mouse_report->y = -mouse_report->y;

#ifdef PS2_MOUSE_ENABLE_SCROLLING
    // Valid z values are in the range -8 to +7
    mouse_report->v = -(ps2_report->z & PS2_MOUSE_SCROLL_MASK);
    mouse_report->v *= PS2_MOUSE_V_MULTIPLIER;
#endif

#ifdef PS2_MOUSE_INVERT_BUTTONS
    if (left_button) mouse_report->buttons |= MOUSE_BTN2;
    if (right_button) mouse_report->buttons |= MOUSE_BTN1;
#else
    if (left_button) mouse_report->buttons |= MOUSE_BTN1;
    if (right_button) mouse_report->buttons |= MOUSE_BTN2;
#endif

    if (middle_button) mouse_report->buttons |= MOUSE_BTN3;
}

static report_mouse_t ps2_mouse_get_report_core(report_mouse_t mouse_report) {
    report_mouse_t     new_report = {};
    ps2_mouse_report_t ps2_report = {};

#ifdef PS2_MOUSE_USE_REMOTE_MODE
    uint8_t rcv;
    rcv = ps2_host_send(PS2_MOUSE_CMD_READ_DATA);
    if (rcv == PS2_ACK) {
        ps2_report.head.w = ps2_host_recv_response();
        ps2_report.x      = ps2_host_recv_response();
        ps2_report.y      = ps2_host_recv_response();
#    ifdef PS2_MOUSE_ENABLE_SCROLLING
        ps2_report.z = ps2_host_recv_response();
#    endif
    }
#else
    // Streaming mode
    if (pbuf_has_data()) {
        ps2_report.head.w = ps2_host_recv_response();
        ps2_report.x      = ps2_host_recv_response();
        ps2_report.y      = ps2_host_recv_response();
#    ifdef PS2_MOUSE_ENABLE_SCROLLING
        ps2_report.z      = ps2_host_recv_response();
#    endif
    }
#endif

    ps2_mouse_convert_report_to_hid(&ps2_report, &new_report);

    return new_report;
}

static uint16_t ps2_mouse_get_cpi_core(void) {
    // PS/2 mouse doesn't really support CPI query
    return 0;
}

static void ps2_mouse_set_cpi_core(uint16_t cpi) {
    // PS/2 mouse uses resolution (counts/mm), not CPI
    // Could map CPI to resolution here if needed
    (void)cpi;
}

/* ============================= WEAK DRIVER FUNCTIONS ============================ */

// These are the pointing_device_driver_* functions called by QMK's pointing_device.c
// Keyboards can override these for custom transforms or initialization.

__attribute__((weak)) bool pointing_device_driver_init(void) {
    return ps2_mouse_init_core();
}

__attribute__((weak)) report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    return ps2_mouse_get_report_core(mouse_report);
}

__attribute__((weak)) uint16_t pointing_device_driver_get_cpi(void) {
    return ps2_mouse_get_cpi_core();
}

__attribute__((weak)) void pointing_device_driver_set_cpi(uint16_t cpi) {
    ps2_mouse_set_cpi_core(cpi);
}

/* ============================= EXPORTED CORE FUNCTIONS ============================ */

// Expose core functions for keyboards that override the weak functions
// but still want to call the default implementation.

bool ps2_mouse_init(void) {
    return ps2_mouse_init_core();
}

report_mouse_t ps2_mouse_get_report(report_mouse_t mouse_report) {
    return ps2_mouse_get_report_core(mouse_report);
}

uint16_t ps2_mouse_get_cpi(void) {
    return ps2_mouse_get_cpi_core();
}

void ps2_mouse_set_cpi(uint16_t cpi) {
    ps2_mouse_set_cpi_core(cpi);
}
