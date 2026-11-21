# GPU backend-bound integration gates — 2026-11-22

## Problem fixed

Renderer availability and a provider's installed SDK are not interchangeable. An engine may have a verified Windows D3D12 renderer and a verified FSR/Vulkan adapter, but **not** an integrated FSR/D3D12 adapter. Selecting the D3D12 renderer must not enable the Vulkan-only effect, even when the same vendor supports both APIs elsewhere.

This change makes each effect require **all** of:
1. A source-verified, functioning selected renderer (Windows D3D12 or Windows/Linux Vulkan).
2. The original provider capability flag (hardware/SDK/prepared input verified by the caller).
3. A *separate* `ApiIntegration` bit proving that particular effect is wired into that renderer API.
4. The existing motion/depth/history and frame-generation pacing/scene-input gates where applicable.

The default for each integration bit is false. Explicit unsupported frame-generation requests stay OFF; no silent replacement by another provider or automatic switching of renderer. A selected renderer without any verified effect continues to render natively.

## Tests

The original ten deterministic source-built C++17 policy tests remain. Four additional cases cover:
- A Vulkan-only FSR integration on a system where Windows auto-selects a valid D3D12 renderer. D3D12 must remain native/no-FG; explicitly selecting Vulkan must use FSR.
- Linux Vulkan must not inherit D3D12-only NVIDIA DLSS SR/FG or Reflex readiness.
- D3D12-only Reflex must not be enabled under a valid Windows Vulkan renderer.
- DLSS 5's separate authorized D3D12 neural adapter must fail closed unless its backend binding is verified, in addition to all existing legal/driver/scene requirements.

These are **synthetic selection-policy tests** only. No vendor SDK function is loaded or exercised, no Vulkan/D3D12 upscaling/frame generation actually runs, and no commercial game is present.

## Official integration references (not runtime verification)

- AMD FSR SDK 2.4, Vulkan and D3D12: https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases
- Intel XeSS 3 developer integrations: https://github.com/intel/xess
- NVIDIA Streamline provider SDK and plugin package policy: https://github.com/NVIDIA-RTX/Streamline
- DLSS 5 official SDK access status as of 2026-11-09: https://forums.developer.nvidia.com/t/official-dlss-5-neural-rendering-plugin-access-for-unreal-engine-5-8-2/382731

## Separate upstream findings (metadata, not independently validated functionality)

- `ivall/xcl` merged a DirectStorage cancellation / intermediate-size / physical-page change in commit [72bb284](https://github.com/ivall/xcl/commit/72bb28402206104484faff3c82798abfafa435d3). This is author/source change evidence, **not** a tested production DirectStorage implementation here.
- `omonkeyz/xbox-emulator` described 34 console library exports in [6e9987e](https://github.com/omonkeyz/xbox-emulator/commit/6e9987ecacf6c5a21ff208ecb535bebc02aa6046). Export count alone is not Xbox title compatibility.
- Existing native Windows PE32+ loader and WARP checks remain useful but neither exercises Xbox D3D12.X or GTA VI.

Next technical gate: execute an original D3D12 frame-present/render+readback fixture through the native loaded PE, then validate API-specific FSR/XeSS/DLSS adapters against real hardware and lawful SDK access; develop Linux Vulkan independently. Keep render-FPS distinct from interpolated-frame presentation rate.
