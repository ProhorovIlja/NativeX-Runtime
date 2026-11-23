# Native Windows GTA VI executable — product and architecture contract

**Decision date:** 2026-11-23  
**Status:** target architecture and acceptance criteria; **NOT an implemented or playable GTA VI executable**.

## The product, not a generic Xbox emulator

The requested deliverable is an ordinary **native Windows x64 GTA VI port**: launch `GTA6.exe` directly from Windows Explorer or from an **optional** `GTA6Launcher.exe`. The game runs its compatible machine instructions on the host x86-64 CPU without CPU instruction interpretation. Use native Windows/D3D12 graphics, physical raw mouse and keyboard, XInput/GameInput controllers, host audio, memory, filesystem, resource streaming, real save handling and a modern PC settings UI. The launcher may select or discover an authorized game installation when opened at the user's request; it is **not** an emulator UI, a gameplay process running inside a virtual machine, or a required online client.

The current source tree contains separate native C++ fixtures and library components. **It does not yet contain a title-running GTA6.exe, game loader, RAGE 9 startup integration, or a validated real game's CPU/render loop.** A Windows executable that only reports system capabilities or displays a launcher is not proof the port works.

### Direct invocation

```text
Windows Explorer
  ├─ double-click GTA6.exe
  │    └─ native Windows game process -> native x64 game code
  │         ├─ native Win32/GDK API implementations or statically rebound calls
  │         ├─ native D3D12 graphics, shaders, input, WASAPI/audio, disk and saves
  │         └─ user's authorized game data directory
  └─ optional double-click GTA6Launcher.exe
       ├─ choose or find installation path (user-initiated, bounded)
       ├─ configure graphics/input/audio and manually check updates
       └─ Play -> launch the SAME native GTA6.exe process
```

The process may have ordinary supporting DLLs beside it (for example graphics provider DLLs), as native Windows games do. **"Native" does not mean that every dependency is statically linked into exactly one file.** Neither the launcher nor a background daemon is required to keep the game running. Offline single-player must be a first-class mode; no account, VPN, recurring service or forced Internet access.

## How console x64 code becomes a *Windows-native* executable

Xbox Series runs x86-64 code, but that code is built for a console GameCore/GDK environment. Having the same CPU ISA does **not** make a console binary a ready-to-run Windows EXE. Windows process startup, imports, user-mode services, graphics extensions, memory behavior, file paths, TLS, exceptions and shader formats may differ.

Our desired final deployment is a **native Windows PE32+ game executable**. Several *implementation techniques* can be compared when a verified, authorized version of the game's executable becomes available:

1. **Static port / PE relinking and import rebinding:** preserve compatible machine-code routines; change relevant module imports, metadata, process startup and console-specific call sites to Windows-native implementations. Replace or recompile incompatible shader/runtime sections as required. Recheck instruction, ABI, memory and exception correctness.
2. **Ahead-of-time native recompilation for difficult code regions:** where direct reuse is unsafe, statically transform selected x64 routines or GPU shader programs into native host code. Keep native performance and explicit correctness tests; don't assume binary translation is always faster than directly executing the existing x64 code.
3. **Integrated system-call adapters:** GDK/console semantics that cannot disappear must be implemented by native Windows code and can be **embedded into the final game binary or shipped as standard local Windows DLLs**. Their presence is not the same as emulating the CPU or shipping a general Xbox console emulator, but they remain real technical dependencies. We must never describe a PC launch path as "no compatibility work" if imports and graphics behaviors still require it.

**Choice is empirical:** inspect the real game's imports, PE layout, shader usage and runtime behavior, then select the combination with the best correctness, stability and hardware performance. It is premature to guarantee a complete single-file static rebuild or zero API adaptation without examining the game executable.

The original game source code is unavailable; native execution of already compiled x64 instructions is therefore more plausible than a literal rebuild of the entire RAGE 9 engine from original C++ sources.

## Optional launcher (not the game itself)

The optional launcher is a native Windows application, not an emulation environment.

- Select the game folder explicitly. Optionally scan **only user-approved bounded locations** when the user initiates detection, showing all candidates rather than silently executing a file.
- Validate game version, paths, file presence and non-sensitive hashes/metadata against **known, verified** requirements when available. Do not pretend to identify a genuine GTA VI executable merely from its filename. Do not embed game files or encryption keys.
- User-controlled settings: output resolution/refresh, native render scale and supported upscalers, quality sliders, RT toggling only if a correct non-RT path exists, FPS cap/frame pacing, full keyboard/mouse remapping, gamepad behavior, audio and accessibility.
- Explicit **Check for updates** action only; no silent installs, auto-start background updater, scheduled checks, notification service or telemetry by default. Any download should require user action and verify provenance and signatures.
- Play starts the same native Windows executable with a selected data directory, working directory and versioned validated configuration; allow ordinary Windows shortcut launch without opening the launcher.
- On errors give a readable report: title executable unavailable, unsupported build, missing required assets, unsupported GPU feature, failed native module import, shader compilation failure, save access failure. Never claim gameplay started if only a synthetic fixture ran.
- Existing asset-overlay safety requirements still apply: no unauthorized plugin injection, no untrusted executable replacement, isolation of game originals and modded saves.

The user's original proposal allows automatic folder discovery **when the user invokes the launcher**; that is an on-demand local search, not an unattended monitor or scheduled automation.

## PC gameplay and quality enhancements

After real title startup and save/restore are correct, implement game-integrated settings rather than a superficial external menu. Some features require inspecting RAGE 9 callbacks and mission save-state invariants:

- Real rendered frame time first: 1080p 60 *true simulated/rendered* FPS as an engineering target for Ryzen 5 5600 / RTX 4070 12 GB / 32 GB RAM; no guaranteed numbers before measurement, Frame Generation off by default. CPU load, GPU passes, streaming, VRAM/RAM, shader compilation and p95/p99 frametime profiling.
- Keyboard/mouse native input, camera sensitivity, raw relative mouse, rebinds, menu/text entry, gamepad haptics if verified. Existing original modules are preparatory; real game's action semantics remain unknown.
- DirectX 12 path with optional verified shader upscaling; renderer quality controls and a fallback raster path only if the real game allows it. A settings switch cannot magically remove mandatory ray tracing.
- Safe autosave options only once mission/checkpoint/world persistence formats are understood. Do not invent or directly mutate mission flags.
- Versioned optional mod infrastructure after game asset and script APIs are validated. Two-player private story co-op and cosmetic role/model changes remain **deferred** until native offline single-player works.

## Priority and acceptance

1. **Runtime acceptance on owned test binaries:** an ordinary Windows-native executable exercises PE32+, imports, allocations, exceptions, graphics submission/presentation, input, sound, save/reload and asset streaming end-to-end. This test by itself is NOT proof GTA VI boots.
2. **Real-game title acceptance:** double-click the resulting Windows game EXE; launch/menu/world/missions/audio/input/save/reload work with independently verified correct data. No emulator application, no compulsory launcher; run without Internet or account for offline play.
3. **Performance acceptance:** repeatable GPU/CPU/frametime/VRAM measurements from real game scenes, not synthetic tests or generated frames. Only then claim optimization and compare other native approaches.
4. **Optional launcher acceptance:** starts the identical native game process without altering execution semantics, with manual updates and explicit directory choice.
5. **Later gameplay additions:** optional autosave controls, mission/graphics settings, private story co-op and modding only after correct real-game runtime tests.

## Repository guidance

Continue using `Scarlett-Windows-Compat` as the research/build repository, but **avoid presenting a generic Xbox compatibility layer as the finished user-facing product**. Source-built Xbox-oriented fixtures are scaffolding and evidence for the **Windows-native GTA VI executable**, not an independent shipping objective. Any future merger must respect original licenses and avoid importing proprietary game assets.

No game files, console SDK binaries, ripped executable, periodic task, background monitor, unattended updater or online service is added by this document.
