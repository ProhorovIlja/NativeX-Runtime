# Native PC graphics SDK and experimental DLSS 5 integration gates

**Evidence date: 2026-11-22.** Windows x64 D3D12 is the primary target; native Linux ELF Vulkan is a second target, while Proton is optional and remains a compatibility layer rather than an independent ELF game port. No GTA VI Xbox binary, materials, motion-vector buffer, scene graph or real frame pacing data are available for game-specific integration.

## Verified official public release metadata

| Provider | Public code / SDK | Latest repository release seen on 2026-11-22 | Interpretation |
|---|---|---|---|
| NVIDIA | [Streamline](https://github.com/NVIDIA-RTX/Streamline/releases/tag/v2.14.16) | **2.14.16**, published 2026-10-16 | Official source and interfaces for integration of supported vendor effects, with feature support gated by GPU, driver, runtime and display; **not automatic DLSS 5 neural rendering** |
| NVIDIA | [DLSS SDK](https://github.com/NVIDIA/DLSS/releases/tag/v310.9.16) | **310.9.16**, published 2026-10-16 | Public SDK release. SDK artifacts and binary distribution have licensing terms; a version label does **not** establish availability of every future neural-rendering plugin |
| AMD | [FidelityFX SDK](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v2.4.0) | **2.4.0**, published 2026-10-28 | SDK includes newer FSR Redstone ML features. For example FSR Frame Generation Swapchain 4.0.0 supports driver-assisted pacing in DirectX 12/Vulkan **on supported AMD RX 9000 Series or newer GPUs and drivers**. Do not present this as universally supported |
| Intel | [XeSS SDK](https://github.com/intel/xess/releases/tag/v3.0.2) | **3.0.2**, published 2026-07-24 | Release notes update **XeLL 1.3.2.10**, including a `xellGetSleepMode` unit fix and a compatibility/memory fix. Intel and non-Intel GPU behavior must still be capability-tested |

These are metadata and vendor announcements, **not** compiled integrations or measured game benefits in this repository. The repo currently contains original Windows WARP D3D12 raster, DXGI Present/frame latency, and native Linux Mesa Vulkan raster readback fixtures. They do not expose real game motion vectors, optical flow, temporal history or full-screen frame generation.

## DLSS 5 is a separate feature family

NVIDIA publicly described [DLSS 5 as 3D-guided neural rendering in September 2026](https://developer.nvidia.com/blog/whats-new-for-game-developers-dlss-5-with-3d-guided-neural-rendering-nvidia-ace-updates-and-new-rtx-kit-capabilities/), with per-engine rendering data and artist-controlled strength/masking. The [October 2026 NVIDIA product update](https://www.nvidia.com/en-eu/geforce/news/dlss-5-faster-ai-model-hogwarts-legacy-rtx-spark-laptops/) documents released/announced supported integrations and RTX Remix availability. This is **not interchangeable** with ordinary DLSS Super Resolution, Ray Reconstruction, DLSS Multi Frame Generation or a driver-level scaling toggle.

At least as of [NVIDIA's November 9, 2026 developer forum response](https://forums.developer.nvidia.com/t/official-dlss-5-neural-rendering-plugin-access-for-unreal-engine-5-8-2/382731), **general developer access to the standalone DLSS 5 neural-rendering SDK had not been publicly announced**. The presence of Streamline 2.14.16, NVIDIA/DLSS 310.9.16, third-party repositories, adapters or leaked patching tools cannot by itself override that licensing or accessibility boundary.

Some GitHub/community tools use modified or leaked `nvngx_dlssnr.dll` versions and modify GPU instructions to run on hardware not officially supported. Such tools are **experimental, not official compatibility guarantees**. Do not vendor leaked binary blobs, unknown download links, unsigned driver components or copied implementation code into this MIT project. Any later optional integration must have a lawful redistributable source, documented dependencies, isolated/opt-in feature flags, user-facing rollback and an independent stability/visual test plan.

## Actual engineering architecture and acceptance gates

1. **Native renderer before plugins:** obtain an authorized build and map real Xbox D3D12.X render passes, RDNA2 shaders, resource descriptor bindings, image formats, depth, temporal buffers, motion vectors, jitter/exposure and final presentation. The original Windows DXGI WARP and Linux Vulkan fixtures are *platform proofs*, not game graphics.
2. **Feature registry, not code-merging everything:** choose platform D3D12 vs Vulkan, query GPU/driver/OS SDK support and per-feature context, and enable a provider only if the exact supported SDK is present. Do not assume vendor A's frame generation can be mixed with vendor B's temporal reconstruction without image-guide correctness and supported composition.
3. **Upscaling:** require stable color, depth, motion vectors, scene cuts, animated transparent elements, temporal reset, frame jitter and HDR handling. Compare source-aligned FSR, DLSS Super Resolution and XeSS by artifact rates, quality, temporal ghosting and throughput; expose clear user control and fallback.
4. **Frame generation:** verify UI composition, camera cuts, interpolation, pacing with display refresh/VRR and engine simulation tick. Separate *generated FPS* from actual simulated frames and latency. AMD's new driver-assisted pacing requires documented GPU/driver support.
5. **Input latency:** the native Windows [DXGI one-frame waitable chain](NATIVE_WIN_DXGI_LATENCY_2026-11-22.md) and [QPC frame timeline](NATIVE_PC_FRAME_TIMING_2026-11-22.md) are basic instrumentation. They **do not** implement Reflex, XeLL or Anti-Lag and have not measured photons. Vendor latency APIs require true game event markers, GPU/hardware support and repeatable physical measurements.
6. **Windows and Linux:** Windows x64 is the main acceptance gate; a Linux ELF/Vulkan path must integrate its own native APIs. Proton/Wine/DXVK/vkd3d-proton are optional compatibility approaches with distinct semantics and tests. Mac is not a currently committed priority.
7. **Licensing and reproducibility:** record exact source commit/tag, SDK EULA/license, binary provenance, SHA, redistribution restrictions, driver support, package signature, minimal required system and clean rollback for every optional provider. Never silently download a provider at launch.

## Evidence that is still absent

Real game rendering, real Rockstar motion/depth guides, title shader translation, physical AMD/NVIDIA/Intel GPUs, DLSS 5 SDK integration, FSR 2.4 ML driver-assisted path, XeSS 3 runtime execution, NVIDIA Reflex, user-mode driver permissions, DirectStorage game resource decompression, HDR/VRR and p95/p99 physical input-to-photon validation are **all unverified**. No project should claim that merely loading a DLL makes these features work in GTA VI. This matrix is a current research boundary, not a claim that these technologies are all implemented.

No schedule, external monitoring service or automatic download mechanism was added by this research.
