# Quality-first 1080p / 60 real FPS profile — 23 November 2026

## Real target, not a hardware-minimum guess

Reference *user-supplied* host: **AMD Ryzen 5 5600, GeForce RTX 4070 12 GB, 32 GB RAM, Samsung NVMe SSD, 1920×1080 VA 240 Hz monitor.** The wired network connection is not a determining factor for offline game FPS. A 240 Hz display is compatible with 60 real FPS; high refresh alone does not require 240 FPS rendering.

**Target:** 1920×1080 output, stable **60 native rendered/simulated FPS**, without Frame Generation, with native resolution or DLSS Super Resolution Quality, rasterization where the game permits, reasonable image quality, responsive controls and bounded resource streaming. An optional Balanced upscale mode is user-selectable after measured image-quality evaluation. **This target is a planned acceptance threshold, not a measured GTA VI result.** No official PC requirements or independently verified native GTA VI execution have been established in this project.

**Do not encode a supposed Ryzen 5600/30 FPS hard floor** or promise 60 FPS on any CPU. An actual simulation/engine tick may be CPU bound regardless of resolution, upscaling or display frame generation. An Xbox game executable is not source code: changing the language of host modules does not recompile a closed-source RAGE engine or automatically accelerate its AI/physics/scripting.

## Source-only profile contract

The original C++17 [profile header](../src/native_1080p60_profile.hpp) and [implementation](../src/native_1080p60_profile.cpp) define a proposed tuning plan, which does **not** change or patch an actual game. The result includes 1920×1080, **60 native-FPS design objective** and explicit booleans `fps_measured=false`, `real_title_tested=false`.

It composes with the existing [native platform feature resolver](../src/pc_feature_policy.hpp), which requires specific backend/provider integration and prerequisites rather than only detecting a DLL or a GPU vendor. Default output preference is **DLSS/FSR/XeSS Super Resolution Quality if a verified provider + native renderer + temporal inputs exist**, otherwise **native resolution**. The user can choose **Native**, **Quality**, or **Balanced** explicitly without silently activating FG. This is an intended integration choice, not proof that an SDK is available or working.

Absolute feature gates:
- **Frame Generation always OFF**, even when the synthetic capabilities advertise a fully integrated DLSS-FG or FSR-FG. Native/actual simulation throughput is the performance metric; generated frames are not counted as game FPS.
- **DLSS 5 neural rendering OFF**. This is a separate 3D-guided appearance pass, **not** DLSS 4.5 Super Resolution. A hypothetical beauty-effect-disabled DLSS 5 is not a known faster DLSS 4.5 Super Resolution replacement. NVIDIA publicly describes structure/tone/masking controls and one-in/one-out appearance enhancement, not a drop-in faster upscaler. RTX 40 support was planned, not confirmed as generally available in NVIDIA's September 2026 announcement.
- **Ray tracing requested OFF**, but accepted *only if* the actual game's verified rendering hook exists **and a complete non-RT raster path is confirmed**. If the title hard-requires RT effects, a checkbox cannot turn them off; rebuilding missing raster paths is separate engine/renderer work. The policy fails closed and never claims RT was disabled when it cannot establish a safe renderer.
- **Reflex** when a working D3D12/Vulkan-compatible adapter, actual backend and relevant NVIDIA driver support have been verified; otherwise safe engine pacing fallback. DirectStorage is similarly conditional on actual Windows support, while io_uring is Linux-only and does not imply Windows DirectStorage parity.
- Per-knob `requested` is an intent and `eligible` is a decision from explicit verified game-hook flags, **not an applied setting**. All game hooks default to false until an executable/source-validated runtime demonstrates them.

## Visual settings: keep detail that matters

| Group | Initial request | Likely limiting component and rationale | Safety/quality guard |
|---|---|---|---|
| Resolution | 1920×1080; Native or SR Quality, optional Balanced | GPU pixels; 1080p SR Quality reduces internal shading work, but image quality depends on fine details | Don't use Ultra Performance as default at 1080p; native may be better when CPU-limited |
| Ray tracing/path tracing | OFF | Often large GPU/RAM/VRAM cost; title may require it | Must prove complete raster alternative rather than disable a necessary game pass |
| Shadows | Low (raise to Medium with headroom) | GPU resolution/filtering + CPU draw submissions | Keep essential dynamic contact shadows for readability |
| Volumetrics | Low | GPU ray marching/3D volume samples | Fog shape and visual atmosphere should remain coherent |
| Reflections | Medium, raster/SSR if supported | GPU; raytraced reflections are an independent toggle | Test missing offscreen reflection fallback |
| Foliage density/distance | Medium/Medium | GPU overdraw/alpha testing, geometry, draw calls and streaming | Do not remove cover or collision relevant to gameplay |
| World object draw distance | Medium; preserve near and distant landmarks with HLOD | Both CPU culling/submit and GPU geometry | Avoid pop-in and incorrect collision/interaction culling |
| Ambient NPC density | Default untouched; optional Medium | CPU agent decisions, animations and pathfinding, plus GPU | **Separate from quest/mission entities**; requires explicitly verified mission-safe hook and user opt-in |
| Ambient traffic density | Default untouched; optional Medium | CPU physics, navigation and driving AI, plus GPU | Requires verified mission-safe routing and user opt-in |
| Texture quality | As high as a measured **12 GB VRAM budget** permits | VRAM and streaming, not necessarily high GPU compute | Prefer high texture sharpness over expensive postprocessing; don't assume 12 GB equals 12 GB free |
| Motion blur, film grain, chromatic aberration, lens distortion | OFF | Usually image clarity first; often small FPS difference | Do not claim a large CPU gain from trivial postprocessing toggles |
| Ambient occlusion, AA, anisotropic filtering | Keep reasonable quality if verified | Mostly GPU; image quality impact significant | Avoid disabling all anti-aliasing or aggressively reducing texture filtering |

The source policy covers the 13 renderer/scene knobs where a meaningful real title hook would be needed. High texture quality, SSAO, anisotropic filtering and HDR are documented as **design preferences** rather than falsely implemented APIs.

## High-leverage engine/performance work by bottleneck

**CPU-bound busy-city frame:** measure game simulation tick duration, main-thread scheduling stalls, jobs, pathfinding, traffic collision/physics, entity wakeups and animation update rates. Use spatial interest management, cheaper **ambient-only** offscreen AI at a tested distance, lightweight far-object animation, broadphase acceleration, event-driven sleeping and cache-friendly allocation. Preserve deterministic/gameplay-critical actors, police response, pursuits, mission scripts, NPC collision and networking semantics. Merely reducing **visual** NPC draw calls won't eliminate CPU simulation load.

**GPU-bound frame:** measure per-pass GPU timing. Reduce unnecessary RT, volumetric samples, shadow atlas cost and expensive reflections. Use HLOD, occlusion culling, instancing/indirect draws, geometric/alpha LOD, smaller memory bandwidth pressure and appropriately chosen DLSS Super Resolution. Turning down render distance can help **both** CPU submission and GPU geometry but may not alter AI/physics load.

**Streaming/VRAM bottleneck:** bounded cache/residency, sensible texture mip selection, asynchronous file reads and decompression, upload buffering, memory aliasing where legal, load prioritization and prefetch based on camera/vehicle trajectory. SSDs and DirectStorage primarily help stalls and IO efficiency, not a guaranteed constant-FPS boost.

**Latency:** optimize engine input sampling, submit/present queue and synchronization; use verified Reflex when available. Keep FG disabled. Frame pacing must be measured with GPU/display statistics in addition to the existing software Present-return timeline; a fast returned Present is not proof a displayed frame was scanned out.

**Language choice:** original C++/Rust/other host components can be judged by correct outputs, memory safety, build compatibility, optimized assembly, allocation and measured cache/branch behavior. Do not assume language replacement or newer instructions alone wins; allow scalar/SSE4.2 fallback using [native x64 CPUID dispatch](NATIVE_X64_CPU_DISPATCH_2026-11-23.md).

## What must be measured before a 60 FPS claim

First obtain a lawful, verified game executable/asset set, real title boot and frame. Then exercise controlled busy city, high speed driving, weather/night lighting, interiors and cutscenes at the same image-quality preset and machine temperature. Capture original rendered FPS, simulation CPU time, GPU time/present statistics, p50/p95/p99 frame intervals, 1% lows, input-latency proxy and hardware-validated display latency where possible, working-set RAM/VRAM, IO spikes, and crash/stutter counts. Compare native, Super Resolution Quality and Balanced without FG, and do not substitute a synthetic 60 target for a measurement.

Official NVIDIA sources checked 23 Nov 2026:
- [DLSS 4.5 Super Resolution available to RTX users](https://www.nvidia.com/en-eu/geforce/news/dlss-4-5-super-resolution-available-now/) (Jan 14, 2026); second-gen transformer model is more computationally demanding, model overrides are user-selectable.
- [DLSS 5 game developer overview](https://developer.nvidia.com/blog/whats-new-for-game-developers-dlss-5-with-3d-guided-neural-rendering-nvidia-ace-updates-and-new-rtx-kit-capabilities/) (Sep 22, 2026); separate 3D-guided neural rendering with Structure/Tone controls.
- [DLSS 5 NVIDIA FAQ](https://www.nvidia.com/en-us/geforce/forums/geforce-graphics-cards/5/583738/dlss-5-faq/) (Sep 3, 2026); GeForce RTX 40 future support intent, not availability guarantee.

**Tests:** [source-built 1080p quality profile scenarios](../fixtures/native_1080p60_profile_tests.cpp) exercise eight synthetic cases: no renderer; verified RTX/DLSS/Reflex with FG flags falsely advertised; Native/Quality/Balanced selection; missing temporal inputs; compulsory RT fallback gate; verified and unverified post-effects; mission-safe ambient NPC/traffic opt-in; Linux Vulkan/FSR/no FG; and no fake real-title FPS declaration. No actual GTA VI settings are changed.

**No game files, proprietary decoder/SDK code, forced AVX2, new scheduled tasks, ongoing monitoring, background agents or external notifications created.**
