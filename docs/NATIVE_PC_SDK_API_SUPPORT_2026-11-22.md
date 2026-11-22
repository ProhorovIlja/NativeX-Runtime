# Official PC SDK API-support gates — 22 November 2026

This note backs the source-built, **synthetic-only** feature resolver `src/pc_feature_policy.cpp`. None of the SDK feature flags in the fixture proves that a provider DLL is installed, licensed, accessible or integrated with an actual render graph.

## Intel XeSS 3 — API and operating system constraints

Official Intel [XeSS 3 developer matrix](https://www.intel.com/content/www/us/en/developer/topic-technology/gamedev/xess.html) documents **XeSS Super Resolution (XeSS-SR) on Vulkan**, while **XeSS Frame Generation (XeSS-FG)** and **XeSS Low Latency (XeLL)** are listed for **DirectX 12**. Therefore the current resolver must reject requested FG and XeLL on the native Vulkan backend, even if a buggy runtime caller advertises both `ready` and Vulkan integration flags. This is a fail-closed rule for the current documented API, not a claim about all possible future versions.

The updated test explicitly injects fake Vulkan `xess_fg` and `xell` flags on Linux and Windows, verifies both effects remain disabled, preserves Vulkan XeSS-SR selection, and shows that properly flagged Windows D3D12 XeSS-SR/FG/XeLL still resolves.

## AMD FidelityFX FSR SDK 2.4

Official [AMD GPUOpen FSR SDK](https://gpuopen.com/amd-fsr-sdk/) and [Khronos Vulkan SDK announcement (2026-10-29)](https://www.khronos.org/news/permalink/amd-fsr-sdk-2.4-brings-fsr-4-upscaling-and-frame-generation-to-vulkan) describe the AMD FSR 2.4 Vulkan integration alongside D3D12, including ML-based upscaling/FG support with hardware and version restrictions. Do **not** hard-disable Vulkan FSR in our resolver merely because an earlier provider version lacked support.

SDK release compatibility does not imply a game automatically receives upgrades. Driver-assisted overrides can depend on a supported prior SDK and swapchain path, and Vulkan support must not be inferred from an existing DirectX integration.

## NVIDIA DLSS 5 distinction

NVIDIA [DLSS 5 developer overview](https://developer.nvidia.com/blog/whats-new-for-game-developers-dlss-5-with-3d-guided-neural-rendering-nvidia-ace-updates-and-new-rtx-kit-capabilities/) describes 3D-guided neural rendering rather than a drop-in upscaler/FG replacement. Public developer forum discussions in November 2026 indicate that public general SDK access is not confirmed; shipping an unapproved binary or enabling a placeholder adapter is not valid integration. The existing policy keeps DLSS 5 distinct from SR/FG and gated by authorized SDK, renderer scene guides, API and GPU driver.

## Acceptance protocol

A feature must require **(1) the selected Windows/Linux renderer exists, (2) the specific renderer API is supported by the current provider, (3) this project has really integrated that provider against the same API, (4) all required temporal motion/depth/history, swapchain and UI separation prerequisites exist, and (5) supported hardware/runtime/licensing is available**. User selection on unsupported hardware should fall back safely and report why. Hardware benchmarks and end-to-end GTA VI render results are not covered by these source fixtures.

No new vendor binaries, unlicensed plugins, monitoring, schedule, or background tasks added.
