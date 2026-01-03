// Copyright 2025 Ira Cooper <ira@wakeful.net>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <stdint.h>
#include <stdbool.h>

// Wrapper protocol prefix
#define WRAPPER_PREFIX 0xDD

// Reserved client IDs
#define CLIENT_ID_BOOTSTRAP  0x00000000
#define CLIENT_ID_ERROR      0xFFFFFFFF

// Error codes
enum client_wrapper_error {
    CLIENT_ERR_INVALID_ID    = 0x01,
    CLIENT_ERR_NO_IDS        = 0x02,
    CLIENT_ERR_UNKNOWN_PROTO = 0x03,
};

// TTL in seconds - short TTL recommended
#ifndef CLIENT_WRAPPER_TTL_SECS
#define CLIENT_WRAPPER_TTL_SECS 120
#endif

// Initialize client wrapper (call from keyboard_post_init)
void client_wrapper_init(void);

// Handle wrapped packet (called when data[0] == WRAPPER_PREFIX)
// Returns true if fully handled, false if VIA should process (for wrapped VIA commands)
bool client_wrapper_receive(uint8_t *data, uint8_t length);

// Allocate a new client ID
uint32_t client_wrapper_allocate_id(void);

// Validate a client ID
bool client_wrapper_valid_id(uint32_t id);
