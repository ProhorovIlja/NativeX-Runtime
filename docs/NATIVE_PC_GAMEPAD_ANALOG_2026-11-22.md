# Native gamepad radial analog handling — Windows and Linux

Original source-built C++17 `src/native_gamepad_analog.{hpp,cpp}` with deterministic `fixtures/native_gamepad_analog_tests.cpp`, 2026-11-22. The only input is an **immutable gamepad state snapshot** from the existing OS-oriented `native_input_core` router; this code does not create synthetic controllers, virtual drivers, inject keystrokes, intercept game inputs or add a second polling thread.

The processor accepts any of four normalized connected controller slots, each with independently configurable **left and right sticks** and two analog triggers. Signed stick endpoints `[-32768,32767]` are normalized symmetrically to [-1,1]. It uses **radial** deadzone processing rather than per-axis clipping: every direction of travel has the same inner-radius threshold and is rescaled continuously toward a configurable outer saturation radius. Full diagonal input is normalized to a unit circle, not accidentally amplified beyond magnitude 1. The optional response choices are linear or cubic smoothstep, plus independent X/Y inversions. Triggers return their continuous 0..255 / 255.0 values.

**Input latency policy:** the calculation is synchronous, stateless and allocation-free. It does not interpolate old frames, smooth input over time, issue hardware output, or add queue latency. An unfocused or disconnected input frame returns a neutral disconnected controller state; invalid slot indices, invalid curve encodings, and inner+outer thresholds that leave no legal movement range are rejected **without changing the caller's previous valid output**.

The Windows/Linux tests cover both signed extrema and diagonals, exact inner deadzone boundary, mid-travel rescaling, early outer saturation, cubic-vs-linear response, axis inversion, four-slot isolation, atomic invalid-config rejection, and focus-loss/hotplug clearing across immutable router snapshots. This is an **engine-side normalized input stage**, not an in-game control menu, calibrated real Sony/Microsoft hardware, a hardware timestamp, mouse camera movement or measured input-to-photon latency.

The saved 1048-byte digital-action binding profile is deliberately **not changed** by this addition; its format must remain backward compatible. Integrating analog curve options into a user settings UI and a versioned controller calibration profile is a separate task after acceptance on real devices.

For physical device access, the separate [native input source and existing Windows adapters](NATIVE_PC_INPUT_2026-11-22.md) cover Raw Input, GameInput and XInput exploration. Actual DualSense **adaptive trigger/haptic feedback** needs a supported opt-in official protocol/API implementation, safe device capability negotiation and physical Windows USB/Bluetooth testing. This change does not claim to provide those output features.

No scheduled jobs, background monitoring, game files, proprietary SDKs or GPL implementation code were introduced.
