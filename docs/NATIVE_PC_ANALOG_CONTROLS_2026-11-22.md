# Native gamepad analog processing — Windows and Linux

`src/native_analog_input.{hpp,cpp}` is an original C++17 preprocessing layer for the existing native input router's immutable per-frame physical `Gamepad` snapshots. It does **not** create virtual controllers, translate mouse movement into sticks, poll platform APIs, include vendor binaries, or assume game-specific controls.

Raw signed 16-bit axes are normalized with separate `-32768` and `+32767` endpoints. A radial inner deadzone suppresses drift, an outer deadzone saturates full-scale response, and an optional linear-to-cubic blend changes sensitivity while preserving angular direction. Separate left/right Y inversion is supported. Trigger floors use an 8-bit threshold; a floor of 255 is handled without division by zero. Sample evaluation has no dynamic allocations.

Only connected gamepads from the selected one of four native router slots produce data. Invalid settings (including NaN or infinity), disconnected pads, missing slots, or focus loss produce neutral values. The configuration values are intentionally not assumed to be game defaults.

Synthetic Linux/Windows CTests cover both signed endpoints, 45-degree diagonal magnitude, intermediate radial scaling, cubic response, focus loss, independent slots, trigger limits, and invalid geometry. This is **not** hardware validation, physical input latency measurement, DualSense adaptive trigger/haptic output, GTA VI action binding, or proof of GTA VI support.

Future: physical USB/Bluetooth hardware testing, live calibration, drift/recenter UI, analog action mapping, input-to-photon instrumentation, game-specific camera/vehicle controls, and device GUID persistence without fragile slot assumptions. No third-party code was copied.
