# Viable QMK Core Patches

This document catalogs all modifications Viable makes to QMK core files (outside of `modules/viable-kb/`).
These changes need to be reviewed and potentially re-applied during QMK rebases.

## Summary

| File | Lines Changed | Risk | Purpose |
|------|---------------|------|---------|
| `quantum/dynamic_keymap.c` | 1 | Low | Weak attr for macro override |
| `quantum/action_util.c` | ~40 | Medium | Dynamic oneshot timeout |
| `quantum/action_util.h` | ~10 | Low | Export `get_oneshot_timeout()` |
| `quantum/keymap_introspection.c` | 12 | Low | Disable static combos/TD/KO |
| `quantum/raw_hid.c` | 1 | Low | Weak attr for HID override |

**Total: 5 files, ~70 lines changed**

---

## Detailed Changes

### 1. `quantum/dynamic_keymap.c`

**Change:** Add `__attribute__((weak))` to `dynamic_keymap_macro_send()`

**Why:** Allows Viable module to override macro execution to support:
- Extended 16-bit keycodes (0x05, 0x06, 0x07 action types)
- Binary delay format
- Complex keycodes (layer toggles, etc.) via `action_exec()`

**Risk:** Low - function signature unchanged, QMK's default still works

```diff
-void dynamic_keymap_macro_send(uint8_t id) {
+__attribute__((weak)) void dynamic_keymap_macro_send(uint8_t id) {
```

---

### 2. `quantum/action_util.c` / `quantum/action_util.h`

**Change:** Add runtime-configurable oneshot timeout

**Why:** Viable exposes oneshot timeout as a QMK Setting that users can change at runtime via GUI, without reflashing.

**Details:**
- Add weak function `get_oneshot_timeout()` that returns compile-time default
- Viable module overrides this to return dynamic value from EEPROM
- Change all `#if (defined(ONESHOT_TIMEOUT) && (ONESHOT_TIMEOUT > 0))` guards to also include `|| defined(VIABLE_ENABLE)`
- Change timeout comparisons from `>= ONESHOT_TIMEOUT` to `>= get_oneshot_timeout()`

**Risk:** Medium - touches multiple locations in the file, but logic is straightforward

```c
// Added weak function
__attribute__((weak)) uint16_t get_oneshot_timeout(void) {
    return ONESHOT_TIMEOUT_DEFAULT;
}

// Changed guards (multiple locations)
-#if (defined(ONESHOT_TIMEOUT) && (ONESHOT_TIMEOUT > 0))
+#if (defined(ONESHOT_TIMEOUT) && (ONESHOT_TIMEOUT > 0)) || defined(VIABLE_ENABLE)

// Changed timeout checks
-return TIMER_DIFF_16(timer_read(), oneshot_time) >= ONESHOT_TIMEOUT;
+uint16_t timeout = get_oneshot_timeout();
+return timeout > 0 && TIMER_DIFF_16(timer_read(), oneshot_time) >= timeout;
```

---

### 3. `quantum/keymap_introspection.c`

**Change:** Disable static introspection for combos, tap dance, and key overrides when `VIABLE_ENABLE` is defined

**Why:** Viable provides these features dynamically via EEPROM, not statically compiled arrays. The static introspection code would conflict.

**Risk:** Low - clean conditional compilation

```diff
-#if defined(COMBO_ENABLE)
+#if defined(COMBO_ENABLE) && !defined(VIABLE_ENABLE)

-#if defined(TAP_DANCE_ENABLE)
+#if defined(TAP_DANCE_ENABLE) && !defined(VIABLE_ENABLE)

-#if defined(KEY_OVERRIDE_ENABLE)
+#if defined(KEY_OVERRIDE_ENABLE) && !defined(VIABLE_ENABLE)
```

---

### 4. `quantum/raw_hid.c`

**Change:** Add `__attribute__((weak))` to `raw_hid_send()`

**Why:** Allows Viable to wrap HID communication for client isolation (multi-client support)

**Risk:** Low - function signature unchanged

```diff
-void raw_hid_send(uint8_t *data, uint8_t length) {
+__attribute__((weak)) void raw_hid_send(uint8_t *data, uint8_t length) {
```

---

## Rebase Checklist

When rebasing onto a new QMK version:

1. [ ] Check if `dynamic_keymap_macro_send()` signature changed
2. [ ] Check if `action_util.c` oneshot handling changed
3. [ ] Check if introspection patterns changed for combos/TD/KO
4. [ ] Check if `raw_hid_send()` signature changed
5. [ ] Re-apply weak attributes if reverted
6. [ ] Run `make svalboard/left:viable` to verify build
7. [ ] Run `git diff --stat master quantum/` to verify patch count
