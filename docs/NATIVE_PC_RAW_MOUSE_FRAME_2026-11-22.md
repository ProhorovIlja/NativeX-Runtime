# Native Raw Input mouse-to-simulation frame accumulation — 2026-11-22

Original C++17 extension of the existing Windows/Linux `native_input_core`: direct unaccelerated relative mouse deltas and signed raw wheel units (horizontal/vertical) are accumulated on the **single input owner thread** for the next simulation frame, without turning physical mouse data into virtual XInput sticks. This is a host input primitive, not a GTA VI camera mapping or measured input latency.

## Owner-thread contract

`Router::mouse_move(device, dx, dy, absolute, ticks)` still records each typed motion event in the existing fixed 512-event diagnostic ring. **Only** relative `MOUSE_MOVE_RELATIVE` reports contribute to `MouseMotion.relative_x/relative_y`. Absolute `MOUSE_MOVE_ABSOLUTE` values remain separately tagged `MouseAbsolute` events and never become camera deltas. Vertical/horizontal wheel input retains raw signed values, not integer notches.

`Router::snapshot(ticks)` remains a nonconsuming immutable held-input snapshot, now also exposing the pending mouse delta. The new `Router::capture_frame(ticks)` returns the full held-input snapshot including pending motion, then clears **only** mouse motion/wheel totals. It does **not** drop or drain keyboard/button/gamepad events or held state. Subsequent captures without new motion return zero motion; a retained previous snapshot is independent.

The accumulator lives outside the bounded diagnostic event ring. Even after **512 queued events overflow**, the frame captures all subsequently received movement/wheel deltas; the separate `dropped_events` field still transparently reports event-ring drops. Its four signed totals **saturate at INT32_MIN/INT32_MAX** instead of integer wrap, with a `saturated` flag; packet counters are uint64 and separately saturate. Under extreme adversarial input, saturation sacrifices magnitude but does not corrupt or reverse sign. Motion originating in the current focused frame is not retrospectively removed on per-device unplug. Focus loss explicitly clears pending motion, held state and stale queue; unfocused input is ignored.

Consumer must process actual OS input, then call `capture_frame()` **once at a chosen simulation frame boundary**. Both calls must use the same input owner thread unless external synchronization is added. `snapshot()` is for nonconsuming inspection. No extra polling thread, per-event heap allocation, intentional smoothing, buffering frame, XInput-emulation layer, or background service was added.

## Executable evidence and limits

Four new scenarios extend the existing `native_input_core_tests` in Windows/Linux CTest from 11 to 15:
1. Relative vs absolute distinction, two physical mouse IDs, independently signed vertical/horizontal wheel.
2. Snapshot immutability, exactly-once frame capture and preservation of event ring and held buttons.
3. 639 relative packets with 512 ring capacity; no mouse deltas lost despite 127 dropped diagnostic events; Alt+Tab/focus reset.
4. Both positive/negative signed INT32 saturation and per-frame reset.

The actual Win32 `native_win_input_probe --interactive` now calls `capture_frame()` on its existing foreground 8 ms diagnostic timer and reports captured mouse packet and frame totals; the headless CI mode remains hardware-free. **An 8 ms diagnostic timer is not a frame pacing recommendation or an input-to-photon measurement.** A full game loop should capture at its own simulation tick and use its own render/present timestamps.

Source guidance: [Microsoft RAWMOUSE relative/absolute and wheel semantics](https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-rawmouse), [WM_INPUT foreground cleanup contract](https://learn.microsoft.com/windows/win32/inputdev/wm-input), [SDL3 GetRelativeMouseState per-frame tradeoff](https://wiki.libsdl.org/SDL3/SDL_GetRelativeMouseState).

Not verified: physical high-Hz mouse timing and USB handling, controller haptics/adaptive triggers, game camera/vehicle semantics, Linux-specific evdev/Wayland adapter, game-title executable, actual input-to-present latency. No proprietary game files, extra input drivers, raw key logs, automatically scheduled jobs or monitoring added.
