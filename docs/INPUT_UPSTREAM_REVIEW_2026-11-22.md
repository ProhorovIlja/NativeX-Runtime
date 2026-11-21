# Native PC input upstream review — 2026-11-22

Goal: improve actual **native** Windows x64 and Linux ELF input pathways without joystick emulation, fake XInput devices or untested proprietary HID output reports. Daily read-only metadata watch uses `sources/input_upstreams.json` and snapshots commit movement, **not** claims of a functional GTA VI controller subsystem.

**Candidates and disposition**

| Source | Role | Project use now | Exclusions |
|---|---|---|---|
| [microsoftconnect/GameInput](https://github.com/microsoftconnect/GameInput) | Microsoft PC input API v4, controllers, keyboard/mouse, haptics; public DualSense helper from 2026-11-17 | Two unchanged MIT-licensed individual source headers pinned to upstream; C++ interface tests | No GameInput runtime DLL redistributed; no Bluetooth trigger framing; no verified physical haptics |
| [libsdl-org/SDL](https://github.com/libsdl-org/SDL) | Zlib-licensed SDL3 cross-platform controllers/HIDAPI and native Linux SDL/Wayland/X11 pathways | Evaluate as Linux input backend and device mappings, no copied code | No mandatory conversion of DualSense into a virtual Xbox pad; don't enable simultaneous duplicate input providers |
| [libusb/hidapi](https://github.com/libusb/hidapi) | Cross-platform raw HID access library | Research/benchmark only until proper per-file license review, transport report framing and QA | Do not send proprietary/unknown output packets blindly |
| [nondebug/dualsense](https://github.com/nondebug/dualsense) | Community USB/Bluetooth descriptors, controller feature reference | Metadata/research, no source copied or imported | GitHub license metadata unverified; documentation is not official Sony SDK |

All four sources pinned to the **observed public HEAD on 2026-11-22**. The watcher retains `initial_baseline_sha` and does not silently rebaseline after a new HEAD. New commits are leads to review, not new features automatically integrated.

**Measured blockers**

- The existing Windows input fixture proves Win32 foreground Raw Input registration and OS XInput 1.4 calls, but not controller/hardware latency, actual game input remapping, adaptive triggers, haptics, touchpad, gyro, battery, headset/microphone, Sony lighting or Bluetooth behavior.
- GameInput v4 makes first-party physical-device support a better candidate than XInput-only. On Windows, one provider must own each device and avoid duplicate XInput/GameInput delivery.
- Linux native ELF is a separate target; Proton runs Windows software and is not a shortcut to an independent native Linux port.
- Detailed game bindings must follow real lawful source game content and engine code, unavailable at this stage.

**Safe progression:** review input device enumeration and grouping, add a real GameInput runtime probe and explicit USB sample-helper capability gates, then native Linux SDL3/evdev input, physical hardware matrix and latency measurements. Optional advanced feedback will remain disabled when transport, capability, user consent, or device safety cannot be demonstrated. Emulation-based input shims such as ViGEmBus are explicitly excluded from the native target and are not watched as integration candidates.
