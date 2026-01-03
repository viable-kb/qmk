// Copyright 2025 Ira Cooper <ira@wakeful.net>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "client_wrapper.h"
#include "viable.h"
#include "raw_hid.h"
#include "host.h"
#include "timer.h"
#include <string.h>

// Nonce size for bootstrap
#define NONCE_SIZE 20

// ID counter - keeps incrementing to avoid reuse
static uint16_t id_counter = 0;

// State for pending VIA wrapper response
static bool wrapper_pending = false;
static uint32_t wrapper_client_id = 0;
static uint8_t wrapper_protocol = 0;

void client_wrapper_init(void) {
    // Seed counter from timer for some randomness on startup
    id_counter = (uint16_t)(timer_read32() & 0xFFFF);
}

uint32_t client_wrapper_allocate_id(void) {
    // High 16 bits: timer, Low 16 bits: counter
    // This makes IDs time-based for expiry detection and unique via counter
    return (timer_read32() & 0xFFFF0000) | (id_counter++);
}

bool client_wrapper_valid_id(uint32_t id) {
    // Reserved IDs are never valid
    if (id == CLIENT_ID_BOOTSTRAP || id == CLIENT_ID_ERROR) {
        return false;
    }

    // Extract timer component from ID
    uint32_t id_time = id & 0xFFFF0000;
    uint32_t now = timer_read32() & 0xFFFF0000;

    // Calculate age (subtraction handles wrap correctly)
    uint32_t age = now - id_time;

    // Valid if younger than TTL
    return age < ((uint32_t)CLIENT_WRAPPER_TTL_SECS * 1000);
}

// Override raw_hid_send to wrap responses when needed
void raw_hid_send(uint8_t *data, uint8_t length) {
    if (wrapper_pending) {
        // VIA processed the command, response is in data[0..length-1]
        // We need to shift it right by 6 bytes and prepend wrapper header
        // Note: last 6 bytes of VIA response will be truncated (wrapper overhead)

        // Shift response right by 6 bytes to make room for header + protocol
        memmove(&data[6], data, length - 6);

        // Prepend wrapper header
        data[0] = WRAPPER_PREFIX;
        data[1] = wrapper_client_id & 0xFF;
        data[2] = (wrapper_client_id >> 8) & 0xFF;
        data[3] = (wrapper_client_id >> 16) & 0xFF;
        data[4] = (wrapper_client_id >> 24) & 0xFF;
        data[5] = wrapper_protocol;

        wrapper_pending = false;
        host_raw_hid_send(data, length);
    } else {
        // Normal send
        host_raw_hid_send(data, length);
    }
}

static void send_error(uint32_t client_id, uint8_t error_code, uint8_t *data, uint8_t length) {
    memset(data, 0, length);
    data[0] = WRAPPER_PREFIX;
    data[1] = client_id & 0xFF;
    data[2] = (client_id >> 8) & 0xFF;
    data[3] = (client_id >> 16) & 0xFF;
    data[4] = (client_id >> 24) & 0xFF;
    data[5] = 0xFF;  // Error protocol
    data[6] = error_code;
    host_raw_hid_send(data, length);
}

// Returns true if command was fully handled, false if caller should let VIA process
bool client_wrapper_receive(uint8_t *data, uint8_t length) {
    // data[0] = WRAPPER_PREFIX (0xDD), already verified by caller
    // data[1..4] = client_id (little-endian uint32)
    // data[5] = protocol (for wrapped commands) or nonce start (for bootstrap)

    uint32_t client_id = data[1] |
                         ((uint32_t)data[2] << 8) |
                         ((uint32_t)data[3] << 16) |
                         ((uint32_t)data[4] << 24);

    // Bootstrap request: client_id == 0
    if (client_id == CLIENT_ID_BOOTSTRAP) {
        // Request:  [0xDD] [0x00000000] [nonce:20]
        // Response: [0xDD] [0x00000000] [nonce:20] [new_client_id:4] [ttl:2]

        uint32_t new_id = client_wrapper_allocate_id();

        // Nonce is at data[5..24], leave it in place (echo back)
        // New client ID at data[25..28]
        data[25] = new_id & 0xFF;
        data[26] = (new_id >> 8) & 0xFF;
        data[27] = (new_id >> 16) & 0xFF;
        data[28] = (new_id >> 24) & 0xFF;

        // TTL at data[29..30]
        uint16_t ttl = CLIENT_WRAPPER_TTL_SECS;
        data[29] = ttl & 0xFF;
        data[30] = (ttl >> 8) & 0xFF;

        host_raw_hid_send(data, length);
        return true;
    }

    // Wrapped command: validate client ID
    if (!client_wrapper_valid_id(client_id)) {
        send_error(client_id, CLIENT_ERR_INVALID_ID, data, length);
        return true;
    }

    // data[5] = protocol byte
    uint8_t protocol = data[5];

    switch (protocol) {
        case VIABLE_PREFIX: {
            // Viable (0xDF) - handle directly
            // Inner payload starts at data[5] (includes protocol byte)
            uint8_t *inner = &data[5];
            uint8_t inner_len = length - 5;

            viable_handle_command(inner, inner_len);
            // Response: wrapper header preserved, Viable response in inner
            host_raw_hid_send(data, length);
            return true;
        }

        case 0xFE: {
            // VIA - strip header, let VIA process, wrap response
            // Save state for raw_hid_send override
            wrapper_pending = true;
            wrapper_client_id = client_id;
            wrapper_protocol = protocol;

            // Shift VIA command to start of buffer
            // data[6] onwards is the VIA command
            memmove(data, &data[6], length - 6);

            // Return false - caller should let VIA process
            return false;
        }

        default:
            send_error(client_id, CLIENT_ERR_UNKNOWN_PROTO, data, length);
            return true;
    }
}
