# Windows-first native PC technology architecture (November 22, 2026)

**Scope.** Native Xbox Series x86-64 research plus a future Windows PC renderer, a later native Linux build, and opt-in graphics/performance improvements. No actual GTA VI Xbox EXE/assets or console-only D3D12.X runtime are in this project. A source-built Windows PE loader already executed original Win32/GDK-shim/D3D12-WARP fixtures; it has NOT executed an Xbox game.

## Native deployment contract

- **Windows**: native x64 process and instruction execution, Win32 services, D3D12/DXGI as primary renderer, optional Vulkan renderer, host input/audio/network/filesystem, correct desktop shader/assets/saves semantics. Xbox ABI/API/command packets still need independent translation.
- **Linux**: native x86-64 ELF-targeted program or validated static recompilation, Vulkan renderer, native SDL3/PipeWire or ALSA, Linux sockets and io_uring where appropriate. A Windows PE mapped with compatibility DLLs is NOT automatically a Linux-native ELF port. Proton/Wine can serve research comparisons but is not the final no-CPU-emulation, pure native Linux deliverable.
- **No magic DLL upgrade**: installing a provider SDK, Vulkan loader or DirectStorage redistributable does not implement GPU shader conversion, renderer integration, motion-vector generation, input routing or storage streaming.

## Optional graphics technology stack

| Layer | Windows native | Linux native | Required validation |
| --- | --- | --- | --- |
| Renderer | D3D12/DXGI primary, Vulkan optional | Vulkan only initially | Physical GPU, driver, swapchain and actual submitted frame; our prior source-built WARP queue/fence is not Present |
| Temporal SR | DLSS Super Resolution; AMD FSR SDK 2.4; Intel XeSS 3; possibly DirectSR abstraction | FSR Vulkan, XeSS SR Vulkan or other explicitly supported verified path | Full-res motion vectors, depth, exposure, jitter/history, correct color/alpha and reactive resources |
| Frame generation | DLSS FG, AMD FSR FG, Intel XeSS-FG as individually verified SDK adapters | Only independently validated native Vulkan provider paths | Stable frame pacing, valid motion/depth, HUD separation, compatible swapchain; displayed vs simulated FPS separate |
| Latency | NVIDIA Reflex, AMD Anti-Lag 2, Intel XeLL, engine fallback | Vulkan-native low latency extensions and engine pacing where supported | Input/simulation/render/present timestamps, driver and extension checks; do not combine unrelated frame pacing methods |
| Neural enhancement | DLSS 5 3D-guided neural rendering as a separate, optional adapter | No claimed implementation | Authorized SDK/runtime, supported NVIDIA device and driver, engine scene guides, approved integration, measured output |
| Display | HDR10/scRGB, SDR fallback, VRR/tearing, fullscreen/windowed | Native Wayland/X11/compositor/driver support, Vulkan WSI | Present cadence, tone mapping, HDR metadata, monitor capability, UI compositing, image comparison |
| GPU resources | Explicit barriers, heaps, descriptors, PSO/shader cache and eviction | Vulkan pipelines, descriptor management, native memory allocator | Device loss/recovery, memory budgets, cache invalidation and shader/format correctness |

**Verified external technology facts, not project capabilities:**
- AMD GPUOpen describes FidelityFX FSR SDK 2.4 and Vulkan feature parity for its relevant upscaling and frame-generation effects. See https://gpuopen.com/amd-fsr-sdk/ and https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK .
- Intel XeSS 3 includes Super Resolution, XeSS-FG and XeLL, subject to per-device API, shader model and mode constraints. See https://github.com/intel/xess .
- NVIDIA Streamline provides DLSS Super Resolution, DLSS Frame Generation, Reflex and Vulkan/D3D integration materials, but feature DLLs and licenses are separate: https://github.com/NVIDIA-RTX/Streamline .
- Microsoft DirectSR standardizes a D3D12 super-resolution API that can expose multiple providers, but it does not automatically deliver every mode on every device: https://microsoft.github.io/DirectX-Specs/DirectSR/DirectSR.html .
- NVIDIA states DLSS 5 with 3D-guided neural rendering shipped in selected RTX 50 games. On November 9, 2026 an NVIDIA developer said the official DLSS 5 SDK was not yet generally publicly accessible. It is a separate one-frame-in/one-frame-out neural rendering technology, not a renamed upscaler or frame generator. See https://developer.nvidia.com/blog/whats-new-for-game-developers-dlss-5-with-3d-guided-neural-rendering-nvidia-ace-updates-and-new-rtx-kit-capabilities and https://forums.developer.nvidia.com/t/official-dlss-5-neural-rendering-plugin-access-for-unreal-engine-5-8-2/382731 .

## DLSS 5 and GitHub mods: no unsafe dependency

The community project MotionflowOffical/UniversalDLSS5 v0.5.1 describes a Windows injector/controller that captures temporal guides and hooks presentation. Its exact SHA and author-claimed functions are tracked separately in sources/experimental_mods.json. We inspected the source README/release, NOT its runtime correctness or binary trustworthiness. We have not imported it.

A public injector implementation and screenshots do not authorize distribution of NVIDIA neural runtimes, leaked proprietary binaries, signature bypass, anti-cheat circumvention or attaching unknown code to commercial games. This project's eventual DLSS 5 adapter requires a legitimate authorized SDK path and tested scene inputs, defaults OFF, and must always be removable without modifying game executables. Until such a path is accessible, it remains an experimental research item, not a promised enabled feature.

## Native non-rendering infrastructure

| Subsystem | Windows implementation candidate | Linux implementation candidate | Acceptance |
| --- | --- | --- | --- |
| Storage | DirectStorage when really integrated, else native overlapped I/O / IOCP | io_uring when supported; portable pread/thread pool fallback | Real content format, request ordering/cancelation, checksum, streaming throughput and 99th-percentile stalls |
| Compression | GDeflate or format-specific decoder if actual lawful assets use it | Portable GDeflate/codecs with per-file license review | Correct decompression hash for legal fixtures; do not assume GTA VI uses any codec |
| Memory | VirtualAlloc2, MapViewOfFile3, D3D12 allocator and residency budgets | mmap/memfd, Vulkan memory budgets/allocator | Aliasing, correct page granularity, OOM handling and GPU residency |
| Audio | XAudio2/WASAPI and spatial audio where supported | PipeWire/ALSA native, SDL3/miniaudio candidates | Capture/playback, surround/HRTF, device changes, underrun count, latency |
| Input | GameInput/Raw Input/XInput/keyboard/mouse | SDL3 native Wayland/X11/evdev | Hotplug, input timestamp, remapping, latency, focus, supported gamepads |
| Networking | Winsock2 UDP/IOCP, congestion policy | Native sockets/epoll/io_uring | NAT fallbacks, encryption/auth, jitter, loss/reconnect, deterministic packet traces |
| Saves | Atomic rename, checksums, journal, backup and migration | XDG dirs, fsync, atomic rename/checksum | Crash-consistent checkpoints, rollback and invalid-save handling |
| Scheduling | Win32 job system + ETW/PIX frame timing | Portable C++ worker pool, Vulkan timestamps, perf/ftrace | Thread scaling on real CPU hardware, no CPU busy-wait spikes |

Microsoft DirectStorage is Windows-specific even though the companion GDeflate codec code can be compiled on Linux. DirectStorage cannot be enabled merely by dropping dstorage.dll into a directory. See https://github.com/microsoft/DirectStorage and its GDeflate/README.md .

## Implemented here: original fail-closed selection policy

The current main branch implements a source-built C++17 policy in src/pc_feature_policy.hpp and src/pc_feature_policy.cpp, with **14** cross-platform deterministic scenarios in fixtures/pc_feature_policy_tests.cpp. Every effect must additionally have an independently verified **selected D3D12 or Vulkan API** integration gate; Vulkan-only FSR/DLSS/XeSS/latency never silently propagates into a D3D12 renderer (or vice versa). See [integration gate evidence](GPU_API_INTEGRATION_GATES_2026-11-22.md). This is a **config/decision unit**, not real FSR/DLSS/XeSS/Reflex/DirectStorage integration.

- Uses caller-verified flags representing app renderer/API/provider integration. A library export's existence NEVER automatically marks an app SDK as working.
- Windows defaults to a verified D3D12 renderer; native Linux requires a verified Vulkan renderer; optional native Windows Vulkan is allowed.
- SR, FG, latency, neural rendering and storage are independent settings. Unsupported provider requests fall back or are disabled with an explicit reason; explicitly chosen FG never silently changes vendors.
- Missing temporal inputs, swapchain FG hooks, verified DLL/driver compatibility or approved NVIDIA neural SDK keep those effects disabled.
- DLSS 5 is a separate opt-in neural stage, default OFF, and will not activate without a lawful authorized API, approved NVIDIA GPU/driver, D3D12 and prepared scene guides.
- DirectStorage on Linux and Windows D3D12 on Linux are never misrepresented as native Linux capabilities. Linux's io_uring and Windows DirectStorage fall back to portable async I/O.

The source-built src/native_pc_host_inventory.cpp inventories only the presence of OS library exports: Windows D3D12/DXGI/DirectStorage/Vulkan/XAudio2/GameInput; Linux Vulkan/PipeWire or ALSA/io_uring. Presence is not a functional GPU capability probe and is never fed into the policy as a verified provider-ready assertion.

## Development gates and measurement

1. **Renderer first:** source-built window+Present, resource upload, scene/GUI and deterministic frame capture on Windows; then native Vulkan and Linux.
2. **Scene data:** motion/depth/exposure/color/jitter/UI separation with correctness tests at multiple resolutions and HDR modes.
3. **One effect at a time:** FSR baseline and measured quality/performance, then XeSS/DLSS SR on verified hardware, then FG + Reflex/Anti-Lag/XeLL and only if legal/available DLSS 5 neural.
4. **Storage and audio:** independent async I/O and native playback probes; record real load stalls, memory allocation pressure and underruns.
5. **GPU/CPU profiling:** render/game FPS separately from FG output, end-to-end latency, p50/p95/p99 frame time, CPU/GPU utilization, VRAM, device errors, asset streaming and visual artifacts. Disable effects if they regress real user quality.
6. **Title integration only later:** with complete lawfully available matching Xbox title files, first ensure correct ABI/memory/shader/commands and saves; only then optimize gameplay, story systems, co-op, settings, and cross-platform support.

No actual commercial game performance figures, native Linux title port or feature compatibility claims are made at this stage.

## Verified independent baseline, not performance claims

[PR #28](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/28) passed Linux and Windows CI and introduced the original native feature selection and library-presence inventory. [PR #29](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/29) passed Linux and Windows CI, building actual native threaded file reads (synthetic 2 MiB input and 64 deterministic asynchronous chunks). These are testable infrastructure layers, not integrated GPU effects or a retail game. See [native asset streaming](NATIVE_ASSET_STREAMING.md).
