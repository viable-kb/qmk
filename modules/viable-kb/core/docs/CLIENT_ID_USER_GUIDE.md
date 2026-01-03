# Client ID Protocol User Guide

## What Is This?

The Client ID protocol solves a fundamental problem: **multiple applications can't reliably talk to the same keyboard at the same time**.

When you run the Viable GUI to configure your keyboard, and also have a layer indicator app showing which layer you're on, their messages get mixed up. The keyboard sends a response, but USB HID broadcasts it to *all* listening apps. Your layer indicator might receive data meant for the GUI, corrupting a keymap download.

This protocol gives each application a unique ID. The keyboard includes this ID in every response, so apps can tell which responses are theirs.

## How It Works

### For End Users

If your keyboard and apps support the Client ID protocol, it "just works." You can run multiple keyboard apps simultaneously without interference.

**Requirements:**
- Keyboard firmware with Client ID support
- Applications updated to use the protocol

**What you'll notice:**
- No more corrupted downloads when multiple apps are open
- Layer indicators, RGB controllers, and configuration GUIs coexist peacefully
- More reliable communication overall

### For Developers

#### Quick Start

1. Before sending any commands, bootstrap to get a client ID:

```python
import os
import struct

WRAPPER_PREFIX = 0xDD

# Generate 20 random bytes
nonce = os.urandom(20)

# Send: [0xDD] [0x00000000] [nonce:20]  - client_id=0 means bootstrap
request = struct.pack("<BI", WRAPPER_PREFIX, 0) + nonce
response = hid_send(device, request)

# Parse: [0xDD] [0x00000000] [nonce:20] [client_id:4] [ttl:2]
assert response[5:25] == nonce  # Verify nonce echo

client_id = struct.unpack("<I", response[25:29])[0]
if client_id == 0xFFFFFFFF:
    raise RuntimeError("Bootstrap failed")

ttl_seconds = struct.unpack("<H", response[29:31])[0]
```

2. Wrap all commands with your client ID:

```python
VIA_PREFIX = 0xFE
VIABLE_PREFIX = 0xDF

# Wrapped command: [0xDD] [client_id:4] [protocol:1] [payload...]
def send_wrapped(device, client_id, protocol, payload):
    msg = struct.pack("<BIB", WRAPPER_PREFIX, client_id, protocol) + payload
    response = hid_send(device, msg)

    # Verify this response is for us
    resp_id = struct.unpack("<I", response[1:5])[0]
    if resp_id != client_id:
        raise RuntimeError("Response was for different client")

    # Return inner response (skip 6-byte wrapper header)
    return response[6:]

# Example: Get VIA protocol version
via_cmd = bytes([0x01])  # id_get_protocol_version
inner_response = send_wrapped(device, client_id, VIA_PREFIX, via_cmd)
```

#### Protocol Reference

**Bootstrap (get client ID):**
```
Request:  [0xDD] [0x00000000] [random_nonce:20]
Response: [0xDD] [0x00000000] [random_nonce:20] [client_id:4] [ttl_secs:2]
Error:    [0xDD] [0x00000000] [random_nonce:20] [0xFFFFFFFF] [error_code:1]
```

**Wrapped command:**
```
Request:  [0xDD] [client_id:4] [protocol:1] [cmd] [payload...]
Response: [0xDD] [client_id:4] [protocol:1] [response...]
```

**Protocol bytes:**
| Byte | Protocol |
|------|----------|
| 0xFE | VIA |
| 0xDF | Viable |
| 0xFF | Error |

**Error response:**
```
[0xDD] [client_id:4] [0xFF] [error_code]

Error codes:
  0x01 = Invalid client ID (expired)
  0x02 = No IDs available
  0x03 = Unknown protocol
```

#### Important Notes

**Response filtering:** Always check that the client ID in the response matches yours. If it doesn't, ignore the response and retry - you received someone else's response.

**Don't ask, just do:** Don't pre-check if your ID is valid. Send the request. If you get "invalid ID" error, re-bootstrap and retry.

**Payload size:** The wrapper uses 6 bytes of the 32-byte HID frame:
- `[0xDD]` - 1 byte
- `[client_id]` - 4 bytes
- `[protocol]` - 1 byte

This leaves 26 bytes for your inner protocol. For buffer operations, subtract command overhead:
- Command structure typically uses 4 bytes (cmd + offset + size)
- Leaving 22 bytes for actual data
- Without wrapper: firmware allows 28-byte chunks
- With wrapper: request 22-byte chunks

**VIA backward compatibility:** Legacy VIA commands (`[0xFE]...` without wrapper) still work for compatibility with existing apps. However, they won't benefit from client ID isolation.

**Viable requires wrapper:** Unlike VIA, Viable commands MUST use the wrapper. Unwrapped `[0xDF]...` packets are rejected. This ensures all Viable communication is properly isolated.

## How Expiry Works

The keyboard mixes its internal timer into client IDs. When validating an ID, it checks if the timer component is recent enough. IDs older than the TTL automatically fail validation.

**What you need to do:**
- Nothing special - errors handle expiry gracefully
- If you get "invalid client ID" error, just bootstrap again
- Optional: renew at 70% of TTL to avoid errors

## Troubleshooting

**"Bootstrap failed: nonce mismatch"**
- You received a response meant for another client
- Retry the bootstrap request

**"Response mismatch: expected X, got Y"**
- Another client's response arrived
- Ignore it and wait for yours, or retry

**"Invalid client ID" error from keyboard**
- Your ID expired
- Bootstrap to get a new ID

**Responses seem delayed or lost**
- Make sure you're filtering by client ID
- Other apps' responses might be arriving first
- Increase retry count if needed

## Example: Secondary Client App

A minimal client app that coexists with the Viable GUI:

```python
import hid
import struct
import time
import os

WRAPPER_PREFIX = 0xDD
VIA_PREFIX = 0xFE

class KeyboardClient:
    def __init__(self, device):
        self.dev = device
        self.client_id = None
        self.ttl = 120
        self.last_bootstrap = 0

    def bootstrap(self):
        nonce = os.urandom(20)
        msg = struct.pack("<BI", WRAPPER_PREFIX, 0) + nonce
        msg = msg + b"\x00" * (32 - len(msg))

        self.dev.write(b"\x00" + msg)
        response = bytes(self.dev.read(32, timeout_ms=500))

        if response[5:25] != nonce:
            raise RuntimeError("Nonce mismatch")

        self.client_id = struct.unpack("<I", response[25:29])[0]
        if self.client_id == 0xFFFFFFFF:
            raise RuntimeError("Bootstrap failed")

        self.ttl = struct.unpack("<H", response[29:31])[0]
        self.last_bootstrap = time.time()

    def send(self, protocol, payload):
        if self.client_id is None:
            self.bootstrap()

        msg = struct.pack("<BIB", WRAPPER_PREFIX, self.client_id, protocol) + payload
        msg = msg + b"\x00" * (32 - len(msg))

        self.dev.write(b"\x00" + msg)
        response = bytes(self.dev.read(32, timeout_ms=500))

        resp_id = struct.unpack("<I", response[1:5])[0]
        if resp_id != self.client_id:
            return None  # Not our response

        return response[6:]

    def get_keyboard_value(self, value_id):
        # VIA command 0x02 = get_keyboard_value, needs value_id parameter
        inner = self.send(VIA_PREFIX, bytes([0x02, value_id]))
        if inner is None:
            return None
        return inner[2:]  # Skip echoed cmd and value_id

# Usage
device = hid.device()
device.open_path(b"/dev/hidraw0")
client = KeyboardClient(device)

# Note: Getting current layer requires keyboard-specific custom values.
# This example shows the wrapper pattern - actual value IDs vary by keyboard.
while True:
    # Example: poll a keyboard-specific value
    result = client.get_keyboard_value(0x30)  # hypothetical layer value ID
    if result is not None:
        print(f"Value: {result[0]}")
    time.sleep(0.1)
```

This app can run alongside the Viable GUI without interference. Each app has its own client ID and can identify its own responses.
