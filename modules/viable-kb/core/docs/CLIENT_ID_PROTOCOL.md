# Client ID Protocol Specification

## Problem

When multiple applications talk to a keyboard simultaneously over USB HID, responses can go to the wrong client. USB HID broadcasts responses to all listeners - there's no built-in request/response correlation.

Example: Viable GUI downloads a keyboard definition (many sequential chunk requests). A layer indicator pings for layer state mid-download. The layer response arrives during the definition download, corrupting the data stream.

## Solution

Keyboard allocates client IDs on request. Clients include their ID in every request; keyboard echoes it in every response. Clients filter responses by their ID.

---

# Protocol Specification

This section defines what clients and keyboards MUST do.

## Packet Formats

### Bootstrap (Get Client ID)

```
Request:  [0xDD] [0x00000000] [random_nonce:20]
Response: [0xDD] [0x00000000] [random_nonce:20] [client_id:4] [ttl_secs:2]
Error:    [0xDD] [0x00000000] [random_nonce:20] [0xFFFFFFFF] [error_code:1]
```

- `0x00000000` as client ID means bootstrap request
- Client sends 20 random bytes (160 bits) as nonce
- Keyboard echoes nonce, returns allocated client ID and TTL in seconds
- Client MUST verify nonce matches before trusting response - this is the ONLY way to correlate bootstrap responses
- 160-bit nonce eliminates birthday collision risk for all practical scenarios
- Client ID is little-endian uint32 (valid range: `0x00000001` - `0xFFFFFFFE`)
- TTL is little-endian uint16 (seconds until ID may expire) - short TTL (1-5 min) recommended
- If allocated client_id is `0xFFFFFFFF`, bootstrap failed - next byte is error code

### Wrapped Command

```
Request:  [0xDD] [client_id:4] [protocol:1] [payload...]
Response: [0xDD] [client_id:4] [protocol:1] [response...]
```

- Keyboard MUST echo the client ID in response
- Client MUST verify response client ID matches before processing

### Error Response

```
[0xDD] [client_id:4] [0xFF] [error_code]
```

Client ID from the request is echoed so client can identify their error.

Error codes:
- `0x01` - Invalid client ID
- `0x02` - No IDs available
- `0x03` - Unknown protocol

## Protocol Bytes

| Byte | Meaning |
|------|---------|
| 0xFE | VIA |
| 0xDF | Viable |
| 0xFF | Error |

Additional protocols may be defined (e.g., 0x01 for XAP).

Bootstrap is indicated by `client_id = 0x00000000`, not a protocol byte.

## Client Requirements

1. **Bootstrap before use**: Obtain a client ID before sending wrapped commands
2. **Include ID in requests**: Every wrapped request includes client ID
3. **Filter responses**: Only process responses matching your client ID
4. **Renew before expiry**: Request a new ID before TTL expires (recommend 66-75% of TTL)
5. **Handle expiry gracefully**: If "invalid ID" error received, re-bootstrap

**Philosophy: Don't ask, just do.** Don't pre-check if your ID is valid. Send the request. If you get "invalid ID" error, re-bootstrap and retry. This is simpler and handles edge cases (clock drift, latency) gracefully.

**Renewal is optional.** Timer-based renewal avoids errors but isn't required. If you skip it and your ID expires, you'll get "invalid ID" error - just re-bootstrap and retry. The protocol handles stale IDs gracefully.

**Sleep/wake:** If you can detect sleep, re-bootstrap on wake. If you can't, don't worry - the error path handles it. The only risk is ID collision (your old ID reassigned to another client), which is minimized by keyboard implementations not reusing IDs quickly.

## Keyboard Requirements

1. **Allocate unique IDs**: Each bootstrap returns a different ID
2. **Avoid quick reuse**: Don't reset allocation cursors - keep advancing to minimize collision risk for sleeping clients
3. **Echo client ID**: Every wrapped response includes the request's client ID
4. **Validate IDs**: Reject commands with expired/invalid IDs (error 0x01)
5. **Report TTL**: Bootstrap response includes time until ID may expire

## Protocol-Specific Rules

- **Viable (0xDF)**: Wrapper REQUIRED. Unwrapped `[0xDF]...` packets MUST be rejected.
- **VIA (0xFE)**: Wrapper optional. Legacy `[0xFE]...` packets allowed for backward compatibility.

## Frame Size

- Total HID frame: 32 bytes
- Wrapper overhead: 6 bytes (`[0xDD] [client_id:4] [protocol:1]`)
- Available for inner protocol: 26 bytes

For buffer/chunk operations, subtract command overhead from 26 bytes:
- Viable definition_chunk: `[0x0E] [offset:2] [size:1]` = 4 bytes → 22 bytes for data
- VIA keymap buffer: `[cmd] [offset:2] [size:1]` = 4 bytes → 22 bytes for data

---

# Reference Implementation

This section describes ONE way to implement the protocol. Keyboards may use different approaches.

## Timer-Based IDs

Mix timer value into IDs. Stale clients have wrong timer component - validation fails naturally.

### State Required

```c
static uint16_t id_counter = 0;
#define TTL_SECS 120  // 2 minutes
```

### Allocation

```c
uint32_t allocate_client_id(void) {
    // High 16 bits: timer, Low 16 bits: counter
    return (timer_read32() & 0xFFFF0000) | (id_counter++);
}
```

### Validation

```c
bool valid_client_id(uint32_t id) {
    if (id == 0 || id == 0xFFFFFFFF) return false;

    uint32_t id_time = id & 0xFFFF0000;
    uint32_t now = timer_read32() & 0xFFFF0000;
    uint32_t age = now - id_time;  // handles wrap

    return age < (TTL_SECS * 1000);
}
```

High 16 bits of ms timer changes every ~65 seconds. IDs older than TTL have stale timer component.

## Dispatch Example

```c
void raw_hid_receive(uint8_t *data, uint8_t length) {
    switch (data[0]) {
        case 0xDD:
            client_wrapper_receive(data, length);
            break;
        case 0xFE:
            via_raw_hid_receive(data, length);  // Legacy VIA
            break;
        case 0xDF:
            // Legacy Viable - REJECTED
            break;
    }
}
```

---

# Client Implementation Example

```python
class ClientWrapper:
    def __init__(self, dev):
        self.dev = dev
        self.client_id = None
        self.ttl = 120
        self.last_bootstrap = 0

    def bootstrap(self):
        nonce = os.urandom(20)
        msg = struct.pack("<BI", 0xDD, 0) + nonce  # client_id=0 means bootstrap
        response = hid_send(self.dev, msg)

        if response[5:25] != nonce:
            raise RuntimeError("Nonce mismatch")

        self.client_id = struct.unpack("<I", response[25:29])[0]
        if self.client_id == 0xFFFFFFFF:
            raise RuntimeError("Bootstrap failed")

        self.ttl = struct.unpack("<H", response[29:31])[0]
        self.last_bootstrap = time.time()

    def send(self, protocol, payload):
        msg = struct.pack("<BIB", 0xDD, self.client_id, protocol) + payload
        response = hid_send(self.dev, msg)

        resp_id = struct.unpack("<I", response[1:5])[0]
        if resp_id != self.client_id:
            raise RuntimeError("Response not for us")

        return response[6:]  # Inner response
```

---

# Viable Protocol Update

## definition_chunk Size Parameter

The `viable_cmd_definition_chunk` (0x0E) command accepts client-specified size:

```
Inner payload: [0x0E] [offset:2] [size:1]
Inner response: [0x0E] [offset:2] [actual_size:1] [data...]
```

When wrapped, the full packet is `[0xDD] [client_id:4] [0xDF] [0x0E] [offset:2] [size:1]`.
The `0xDF` is the protocol byte in the wrapper header, not duplicated in the payload.

Clients request chunk size based on available space (22 bytes when wrapped).

---

# Testing Checklist

- [ ] Bootstrap returns valid client ID
- [ ] Bootstrap nonce is echoed correctly
- [ ] Response includes correct client ID
- [ ] Client can filter responses by client ID
- [ ] Expired client ID returns error 0x01
- [ ] Wrapped VIA commands work
- [ ] Wrapped Viable commands work
- [ ] Legacy VIA (unwrapped) still works
- [ ] Legacy Viable (unwrapped) is rejected
- [ ] Multiple clients get different IDs
- [ ] Client renewal before expiry works
