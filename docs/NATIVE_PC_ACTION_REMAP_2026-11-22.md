# Native Windows/Linux input action binding and remap profile core

Original independent C++17 `src/native_action_bindings.{hpp,cpp}`, with deterministic executable `fixtures/native_action_bindings_tests.cpp` (2026-11-22). It operates only on **immutable per-frame snapshots** from the already source-tested [Raw Input/XInput state router](NATIVE_PC_INPUT_2026-11-22.md), and does not create virtual devices, hooks or emulated console controls.

## Supported mapping and state contract

A fixed-size `Map` contains **32 generic logical action slots**, each with **four independent alternatives**. A binding may read a keyboard scan code including E0/E1 extended bits, one of five mouse buttons, one of 16 digital buttons from any of four normalized gamepad slots, or the left/right trigger with an inclusive 1–255 threshold. Optional held keyboard modifier scan codes allow genuine chords, including across separate keyboards where the aggregate snapshot is active. Binding updates reject invalid code/range/modifier combinations **without altering an existing valid binding**. Empty/unfocused frame snapshots never assert actions; disconnected gamepads cannot activate bindings.

`Map::evaluate` is allocation-free and uses a bounded number of checks. It does not query Windows input APIs while simulating: a separate owner thread has already captured a stable frame snapshot. There is no assumption that the generic controller is connected to a particular Sony/Microsoft hardware model. Remapping is an internal logic layer; a Settings GUI and game-specific command adapters remain to be implemented after a real title can be audited.

## Persistence

`Map::serialize` creates a **fixed 1048-byte** deterministic little-endian version-1 record: 8-byte magic `SCBIND01`, u32 version, u32 reserved zeros, 128 × 8-byte binding entries and u64 **FNV-1a 64** integrity checksum. It does **not** persist device IDs, hardware identity or user-specific paths. The checksum only catches accidental corruption; it is not encryption or authentication.

`deserialize` rejects wrong length, unknown version, bad checksum, invalid codes or noncanonical disabled entries. It validates an entire temporary candidate before changing the caller's active profile. The tests round-trip this record through the separately verified native [two-slot journal-like save primitive](NATIVE_PC_SAVE_RECOVERY_2026-11-22.md) in a temporary OS folder, then reopen and evaluate the restored binding. No commercial-game save is used.

## What is not done

This is a standalone Windows/Linux engine-side mapping kernel, **not** a GTA VI controller profile. It has no real mouse-relative-to-camera movement, mouse wheel bindings, analog stick-to-movement scaling, deadzone curves, device provider hotplug deduplication, haptics, adaptive triggers, controller glyph switching, UI editing or physical device latency measurements. No game-specific default mappings should be invented without the game's real control scheme.

For those features, compare native Microsoft GameInput 4/official DualSense sources, SDL3/HIDAPI and actual supported hardware, while respecting their source licenses. Native Windows Raw Input and direct platform APIs remain preferred over controller simulation.
