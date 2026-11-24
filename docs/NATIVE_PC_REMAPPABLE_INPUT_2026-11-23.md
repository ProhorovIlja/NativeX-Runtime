# Native PC remappable action input — 2026-11-23

## Scope

This original Windows/Linux C++17 module connects the **existing native physical-input state machine** to a deterministic, remappable PC action layer. It is a concrete building block for the requested fully native GTA VI PC experience, not an emulation of an Xbox controller and not a modification to any commercial game executable.

The current [Raw Input + XInput host probe](../fixtures/native_win_input_probe.cpp) uses real Win32 Raw Input / XInput APIs and the [platform-independent Router](../src/native_input_core.hpp) already tracks multiple physical keyboards/mice, 4 gamepad slots, relative raw mouse displacement, wheel input, focus loss and disconnected devices. The new [action mapper](../src/native_input_bindings.hpp) deliberately consumes **immutable per-frame snapshots**, so host-specific OS event processing stays separate from gameplay semantics.

No RAGE 9 keyboard/action ABI is available, so [desktop_example](../src/native_input_bindings.cpp) is explicitly an illustrative PC layout and **not a claimed GTA VI control scheme or working game integration**.

## Implemented interfaces and behavior

- 13 generic PC actions: move forward/back/left/right, sprint, jump, interact, attack, aim, reload, pause, menu accept/back. Two independent bindings per action; supported native sources are hardware scan codes (including E0/E1 extended indices from Raw Input), mouse buttons 1–5 and one-bit XInput gamepad buttons. Per-context Gameplay/Menu routing makes overlapping bindings legal across different contexts but rejects same-context collisions.
- Fully rebindable in-memory action mapping. Malformed action IDs, nonexistent slots, zero/out-of-range keyboard codes, invalid mouse buttons, unsupported controller masks and duplicate same-context physical inputs are rejected. No untrusted keyboard bindings or arbitrary scripts are executed.
- Each simulation-frame result includes down/pressed/released edges. Keyboard auto-repeat does not create new action press events; simultaneous physical keyboards and two gamepads preserve OR semantics. Device unplug releases the action only when *all* devices for that input have released.
- On losing window focus, actions release and camera look returns zero. Changing contexts or editing bindings re-arms: an already held key cannot instantly trigger a new action in the newly opened menu (requires release and repress). This avoids common stuck controls and accidental menu selection after Alt+Tab.
- Gameplay mouse look consumes raw relative integer motion accumulated by the existing Router; no mouse-to-analog-stick translation and no OS pointer acceleration is introduced. Adjustable finite sensitivity in [0.05, 10] and invert-Y, plus raw vertical/horizontal wheel counts. Absolute mouse coordinates are **not** converted to camera motion. UI context does not consume motion for gameplay camera.
- Input event ring saturation does not destroy the per-frame mouse displacement; the Router's independent saturating accumulator feeds the mapper. No input is stored to disk; no keylogging, global hooks, input injection, background work, installed service or task.
- The mapper is a small fixed-size state machine with no allocations on its evaluation hot path. It runs on the *input-owning frame thread*, not as a separate worker.

## Important boundaries

1. **Short taps are now preserved at frame resolution (PR #109 candidate)**: the Router captures independent aggregate press/release edge bitsets for physical keys/mouse buttons and a mask for controller buttons even if diagnostic events overflow. A key pressed and released completely between two simulation frames now emits both `pressed` and `released` in the same action frame, with `down=false`. Capture clears edges once; non-consuming snapshot reads do not. **Not fully lossless input history:** two or more taps within one frame coalesce, edge ordering/timestamps are not preserved, a release/repress while an action is already held can be coalesced, and XInput polling cannot detect taps shorter than the poll interval. Future title integration needs timestamped ordered per-frame events and verified simulation semantics, not just edge flags.
2. **Analog movement/aim and deadzones**: actual XInput analog stick and trigger values are retained by the lower-level Router but are not mapped to movement axes by this digital action mapper yet. Analog response/deadzone tests exist separately. Controller haptics/advanced DualSense effects require verified runtime support.
3. **Bindings are not saved**: persistence requires versioned, validated user configuration without encoding physical device IDs, and explicit UI support for conflict resolution. There is no invented user menu or game hook.
4. **Measured input latency**: the snapshot has a receipt timestamp, but this module does not show real game input-to-photon latency. A 240 Hz monitor is not proof of a 240 Hz simulation or a particular latency level.
5. **Real game semantics**: key names, camera transform, action-to-script mapping, pause/menu behavior, gameplay networking, online-safe hooks and animation/state machines require actual GTA VI executable research. This module cannot operate GTA VI alone.

## Acceptance checks

[Twelve portable source-built scenarios](../fixtures/native_input_bindings_tests.cpp) cover held and auto-repeat keyboard edges; simultaneous physical keyboards/disconnect; raw motion, optional invert-Y, absolute input and wheels; gameplay/menu isolation and Alt+Tab release; multiple XInput controllers; explicit rebind conflicts and invalid inputs; raw event-queue overflow; menu actions and action names; **complete short keyboard, mouse and controller taps between frame captures**; multi-device aggregate edge correctness despite unplug; quick clicks despite event-ring overflow; and focus/rebind immunity against replayed stale edges. All integration tests use synthetic OS-independent Router inputs, not real game code, and do not require controller hardware.

### Related integration priorities

Native Windows x64 first; real game-compatible action/simulation dispatch after the console executable is available and verified. Use CPU/graphics/streaming bottleneck profiling to prioritize work. Allow user profiles with raw input, complete rebinds, separate UI/gameplay settings, optional mouse acceleration *only if explicitly desired*, gamepad/keyboard coexistence, and per-user camera preferences. Keep Frame Generation OFF by default with measured real rendered/simulated FPS in the requested [1080p/60 quality policy](https://github.com/ProhorovIlja/NativeX-Runtime/blob/14a8806993bcd73887c63c5261ae4d62598f759b/docs/GTA_VI_1080P60_QUALITY_NO_FG_PROFILE_2026-11-23.md).

No game executable or assets were imported, no proprietary SDK or GPL code was copied, and no scheduled/ongoing work was established.
