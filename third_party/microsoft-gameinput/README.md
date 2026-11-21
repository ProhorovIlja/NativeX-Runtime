# Official Microsoft GameInput v4 and DualSense source-only integration

The following two **unaltered** MIT-licensed Microsoft source headers are vendored for reliable offline compilation:
- `GameInput.h` from [Microsoft GameInput](https://github.com/microsoftconnect/GameInput/blob/4a3150cbe27d5fa7bad98dbacd79a40152e4b05e/include/GameInput.h), source Git blob `f149a60321d117c5a55b574ad1f4d9663abd1542`.
- `DualSenseHelper.h` from [Microsoft GameInput DualSense sample](https://github.com/microsoftconnect/GameInput/blob/4a3150cbe27d5fa7bad98dbacd79a40152e4b05e/companion/DualSense/DualSenseHelper.h), source Git blob `2ec7eb1c72c0d603fb67faa4fa84c8d5bf196f40`.

Pinned upstream commit `4a3150cbe27d5fa7bad98dbacd79a40152e4b05e` (2026-11-17). Original copyright, license notices and disclaimers remain in both files, and are not replaced by the repository-wide license. They are SDK **headers**, not a runtime or controller driver. No SDK DLL, redistributable package, firmware, Sony library, game asset or Xbox system binary is included.

The project compiles `fixtures/native_win_gameinput4_probe.cpp` against these headers with MSVC x64 warnings-as-errors and tests:
- Source-level Sony DualSense `054C:0CE6` and Edge `054C:0DF2` identification and negative IDs.
- Official sample helper rejects malformed trigger effect ranges **before** any device access.
- Optional attempt to dynamically load official `GameInputRedist.dll` from **System32 only**. If present and it exports `GameInputInitialize`, call the real GameInput v4 interface and attempt `GetCurrentReading(GameInputKindGamepad)`. On a runner with no configured GameInput runtime/device, it must remain a successful **SDK interface** test, but the JSON fields truthfully show `runtime_initialized=false` or `physical_gamepad_reading=false`.
- No simulated HID/virtual XInput, no controller output, no injection, no background keyboard monitoring.

Manual physical tests are still required on Windows 10/11 with USB and Bluetooth DualSense/Edge for real trigger/haptic effects, connection lifecycle, transport-specific framing and power management. The pinned Microsoft companion helper only covers USB-format report 0x02 and explicitly does **not** support Bluetooth framing/checksums. Do not issue trial output reports to unknown devices. Any future trigger activation must be explicit, bounded to low forces and reverted OFF on focus loss, disconnect, suspend and application shutdown.

The source integration will not become a functioning GTA VI input provider until the real game interfaces are available and device enumeration, de-duplication, input ownership, remapping and full hardware tests are complete.
