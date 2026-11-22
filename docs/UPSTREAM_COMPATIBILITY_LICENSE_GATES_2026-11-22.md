# Xbox Series -> native Windows/Linux: public upstream acceptance and license gates (2026-11-22)

This is a **source/repository inspection and decision record**, not a demonstration that an unowned retail title boots. GitHub repository HEADs below were directly queried on 2026-11-22. A README's results are *upstream author claims*, not Scarlett-Windows-Compat independently reproduced outcomes. These projects target different execution models; do not merge their source indiscriminately.

## Observed current upstream snapshots

| Upstream | Observed HEAD | GitHub license metadata | What is actually useful | Hard gap / reuse gate |
|---|---|---|---|---|
| [Xbox2Windows](https://github.com/R0Y00/Xbox2Windows) | `c2fc84cde4de1aea81b5f45db0bd7c7271e3f43e` | MIT | Source-built x64 PE static recompilation or relocation tests, shader instruction lowering, texture and PSO differential tests; author describes real Windows D3D12/WARP trials | Not title-wide executable compatibility; audit/port individual MIT code only after independent fixture comparisons |
| [GDKScarlett](https://github.com/Ender-0001/GDKScarlett) | `e703bc3a11ad2780c9167f98c25b9867c7b08797` | BSD-2-Clause | Native Windows x64 console-GDK import broker, desktop API shims and shader translation ideas | README explicitly says **Xbox Series `d3d12_xs.dll` games are not supported**. Several Microsoft console graphics/helper DLLs are user-supplied, not redistributable here |
| [XCL](https://github.com/ivall/xcl) | `660f6dc2f0eec8a14cf69c7cae0d8f1813da1fa1` | GPL-2.0 | Rust Xbox x64 loader/GDK, RDNA2 GPU shader lifting, GFX10 texture layout, physical GPU memory windows; author reports extensive retail shader/texture checks | README says full graphics **draw submission/present still needs the actual title EXE**. GPL source must not silently enter the MIT core; numerical/test-oracle ideas need independent lawful clean-room implementation |
| [AnyXboxX](https://github.com/LooperSalty/AnyXboxX) | `273bd9aec63f96a9548717697a0cd942508630cb` | GPL-3.0 (repo metadata) | Separately source-built native test launcher and RDNA2 opcode research; six synthetic PE32+ probes passed in our CI at a prior pinned revision | Newer GPU changes and commercial title compatibility are not independently verified. No automatic GPL-to-MIT code copy |
| [ScarlettX](https://github.com/Freminet/ScarlettX) | `bda18b4d3dab501dd0f372e5d1921f462c6bd457` | No repository-level SPDX license reported / no root LICENSE observed | Useful GDK export inventory, subsystem capability checklist, negative test cases | Author describes **HLE**, with 20/23 tests passing and major shader/UMA/retail format subsystems still incomplete. No source import without obtaining explicit licensing terms |
| [xgameruntime](https://github.com/nocts-software/xgameruntime) | `f1d172b1180d45fff3e3f2b0abad517460023d0f` | LGPL-2.1 | Wine/Proton xgameruntime, task queues, XGameSave and GDK service interfaces for Windows-built PC games on Linux | Does **not** translate Xbox RDNA2 or substitute for a native Linux ELF game. LGPL reuse needs careful dynamic-link/compliance review |

These are **snapshots**, not automatically polled feeds. Record exact upstream commit plus license at the time a feature is evaluated. More permissive is not automatically faster or more complete; a demonstrated smaller implementation is better than an untested large fork.

## Non-negotiable engineering acceptance

1. **Windows first:** direct x64 CPU execution, real PE32+ imports/relocations, SEH/unwind, TLS, loader lifecycle and GDK API semantics. An Xbox API adaptation layer is still required even when there is no CPU emulation. Synthetic MSVC PE acceptance does not validate an Xbox Game Core EXE.
2. **Console graphics:** distinguish desktop D3D12 present/readback (already independently tested with WARP) from `d3d12_xs` command submission, resource layout, special memory residency and RDNA2 shader ISA translation. Require source-built instruction-by-instruction reference tests and physical AMD/NVIDIA/Intel GPU evidence. Do not claim renderer compatibility from a translated shader count alone.
3. **Linux distinct targets:** (a) Wine/Proton execution of a Windows PE path with Vulkan/vkd3d-proton and relevant GDK runtime fixes versus (b) an actual Linux ELF-native build. The latter requires a separately built host executable or recompilation/translation strategy; Proton is not that native ELF route.
4. **Upscalers/latency:** feature policy switches are not SDK integration. Authenticate actual SDK access and runtime permissions; verify matching graphics API adapter, motion vectors, jitter, depth, exposure, HUD/FG separation, frame timing and per-vendor hardware. Never bundle unofficial DLSS 5 binaries or call a config flag an implementation.
5. **Input:** native raw keyboard/mouse events and separately identified physical controllers; controller snapshots, analog curves and game actions are distinct layers. No virtual stick mapping of the mouse. DualSense adaptive triggers/haptics need explicitly supported USB/Bluetooth protocol, tested opt-in device output and safe disconnect handling.
6. **Disk/audio/video/network:** use small deterministic contract tests first, then real NVMe, sound devices, physical frame pacing and game resources. IOCP != DirectStorage, PCM mixer/resampler != codec/low-latency audio, NV12 surface conversion != Bink playback, TCP loopback != multiplayer netcode.
7. **Provenance:** source-visible permissible licensing, pinned commit, own differential reference tests, negative corruption cases, and measured performance/stability regressions before taking any module. A fork's README performance numbers are not measurements of this project.
8. **Actual title gate:** only a lawful accessible, complete title build can establish imports, Xbox shader formats, package layouts, asset decoding, graphics Present and one full playable single-player campaign. Do not distribute or import proprietary assets, consoles keys, firmware, or third-party SDK binaries without appropriate rights. Co-op remains downstream of verified single-player.

## Recommended next experiments without title files

- Windows PE32+ runtime **unwind v1 .xdata** validation and loaded-image correctness beyond read-only metadata. Check overlap with merged PRs before creating more code.
- Additional independent negative and positive **BC7** block decoders and Xbox swizzle geometry fixtures, no assumptions about retail title layout; compare GPU upload and CPU oracle on D3D12/Vulkan.
- Develop real Linux Vulkan WSI presentation gate (with headless skip), then physical-GPU cross-device tests for shader/SPIR-V work.
- Host audio stream buffering and **high-quality bandlimited rate conversion** comparison; never ship linear interpolation as the premium downsampling path.
- A source-built **GDK API conformance fixture** for task-queue cancellation, ordering, errors and ABI under Windows, with cross-platform isolated tests where sensible.

This research note does not initiate monitoring, subscriptions, background jobs or scheduled work. No third-party source or game binary was imported.
