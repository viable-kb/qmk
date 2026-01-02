// Copyright 2011 Jun Wako <wakojun@gmail.com>
// Copyright 2023 Johannes H. Jensen <joh@pseudoberries.com>
// Copyright 2025 Ira Cooper <ira@wakeful.net>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "report.h"
#include "ps2.h"

// Debug output macros
#ifdef POINTING_DEVICE_DEBUG
#    include "pointing_device_internal.h"
#    define PS2_MOUSE_DEBUG(...) pd_dprintf(__VA_ARGS__)
#else
#    define PS2_MOUSE_DEBUG(...)
#endif

// Command send/receive macros
#define PS2_MOUSE_SEND(command, message)                                                          \
    do {                                                                                          \
        __attribute__((unused)) uint8_t rcv = ps2_host_send(command);                             \
        PS2_MOUSE_DEBUG("%s command: %X, result: %X, error: %X\n", message, command, rcv, ps2_error); \
    } while (0)

#define PS2_MOUSE_SEND_SAFE(command, message)          \
    do {                                               \
        ps2_mouse_disable_data_reporting();            \
        PS2_MOUSE_SEND(command, message);              \
        ps2_mouse_enable_data_reporting();             \
    } while (0)

#define PS2_MOUSE_SET_SAFE(command, value, message)    \
    do {                                               \
        ps2_mouse_disable_data_reporting();            \
        PS2_MOUSE_SEND(command, message);              \
        PS2_MOUSE_SEND(value, "Sending value");        \
        ps2_mouse_enable_data_reporting();             \
    } while (0)

#define PS2_MOUSE_RECEIVE(message)                                          \
    do {                                                                    \
        __attribute__((unused)) uint8_t rcv = ps2_host_recv_response();     \
        PS2_MOUSE_DEBUG("%s result: %X, error: %X\n", message, rcv, ps2_error); \
    } while (0)

// PS/2 Mouse Commands
enum ps2_mouse_command_e {
    PS2_MOUSE_CMD_RESET                  = 0xFF,
    PS2_MOUSE_CMD_RESEND                 = 0xFE,
    PS2_MOUSE_CMD_SET_DEFAULTS           = 0xF6,
    PS2_MOUSE_CMD_DISABLE_DATA_REPORTING = 0xF5,
    PS2_MOUSE_CMD_ENABLE_DATA_REPORTING  = 0xF4,
    PS2_MOUSE_CMD_SET_SAMPLE_RATE        = 0xF3,
    PS2_MOUSE_CMD_GET_DEVICE_ID          = 0xF2,
    PS2_MOUSE_CMD_SET_REMOTE_MODE        = 0xF0,
    PS2_MOUSE_CMD_SET_WRAP_MODE          = 0xEC,
    PS2_MOUSE_CMD_READ_DATA              = 0xEB,
    PS2_MOUSE_CMD_SET_STREAM_MODE        = 0xEA,
    PS2_MOUSE_CMD_STATUS_REQUEST         = 0xE9,
    PS2_MOUSE_CMD_SET_RESOLUTION         = 0xE8,
    PS2_MOUSE_CMD_SET_SCALING_2_1        = 0xE7,
    PS2_MOUSE_CMD_SET_SCALING_1_1        = 0xE6,
};

// PS/2 ACK value
#ifndef PS2_ACK
#    define PS2_ACK 0xFA
#endif

/*
 * PS/2 Mouse Data Format:
 * byte|7       6       5       4       3       2       1       0
 * ----+----------------------------------------------------------------
 *    0|[Yovflw][Xovflw][Ysign ][Xsign ][ 1    ][Middle][Right ][Left  ]
 *    1|[                    X movement(0-255)                         ]
 *    2|[                    Y movement(0-255)                         ]
 *
 * if PS2_MOUSE_ENABLE_SCROLLING:
 *    3|[                    Z movement(0-255)                         ]
 */
typedef struct __attribute__((packed)) {
    union {
        struct {
            bool left_button : 1;
            bool right_button : 1;
            bool middle_button : 1;
            bool always_one : 1;
            bool x_sign : 1;
            bool y_sign : 1;
            bool x_overflow : 1;
            bool y_overflow : 1;
        } b;
        uint8_t w;
    } head;
    uint8_t x;
    uint8_t y;
#ifdef PS2_MOUSE_ENABLE_SCROLLING
    uint8_t z;
#endif
} ps2_mouse_report_t;

// Default configuration values
#ifndef PS2_MOUSE_X_MULTIPLIER
#    define PS2_MOUSE_X_MULTIPLIER 1
#endif
#ifndef PS2_MOUSE_Y_MULTIPLIER
#    define PS2_MOUSE_Y_MULTIPLIER 1
#endif
#ifndef PS2_MOUSE_V_MULTIPLIER
#    define PS2_MOUSE_V_MULTIPLIER 1
#endif
#ifndef PS2_MOUSE_SCROLL_MASK
#    define PS2_MOUSE_SCROLL_MASK 0xFF
#endif
#ifndef PS2_MOUSE_INIT_DELAY
#    define PS2_MOUSE_INIT_DELAY 1000
#endif

// Resolution values (counts per mm)
typedef enum ps2_mouse_resolution_e {
    PS2_MOUSE_1_COUNT_MM = 0,
    PS2_MOUSE_2_COUNT_MM = 1,
    PS2_MOUSE_4_COUNT_MM = 2,
    PS2_MOUSE_8_COUNT_MM = 3,
} ps2_mouse_resolution_t;

// Sample rate values (samples per second)
typedef enum ps2_mouse_sample_rate_e {
    PS2_MOUSE_10_SAMPLES_SEC  = 10,
    PS2_MOUSE_20_SAMPLES_SEC  = 20,
    PS2_MOUSE_40_SAMPLES_SEC  = 40,
    PS2_MOUSE_60_SAMPLES_SEC  = 60,
    PS2_MOUSE_80_SAMPLES_SEC  = 80,
    PS2_MOUSE_100_SAMPLES_SEC = 100,
    PS2_MOUSE_200_SAMPLES_SEC = 200,
} ps2_mouse_sample_rate_t;

// PS/2 Mouse control functions (for keyboard overrides to use)
void ps2_mouse_disable_data_reporting(void);
void ps2_mouse_enable_data_reporting(void);
void ps2_mouse_set_remote_mode(void);
void ps2_mouse_set_stream_mode(void);
void ps2_mouse_set_scaling_2_1(void);
void ps2_mouse_set_scaling_1_1(void);
void ps2_mouse_set_resolution(uint8_t resolution);
void ps2_mouse_set_sample_rate(uint8_t sample_rate);

// Core driver functions (for keyboard overrides that call the defaults)
bool ps2_mouse_init(void);
report_mouse_t ps2_mouse_get_report(report_mouse_t mouse_report);
uint16_t ps2_mouse_get_cpi(void);
void ps2_mouse_set_cpi(uint16_t cpi);
