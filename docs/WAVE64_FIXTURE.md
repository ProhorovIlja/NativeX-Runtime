# Wave64 → two wave32 CPU differential fixture

This original C++17 fixture validates *logical* 64-lane computations against a split two-32-lane model on deterministic synthetic inputs. The reference provides:

- active-lane ballot mask;
- reads from arbitrary lanes, including lane 31 ↔ 32;
- cross-half XOR-32 exchange;
- 64-wide wrapped-u32 total and inclusive prefix;
- invalid/inactive source reads marked *undefined*, not assumed to be zero.

The split model uses per-32-lane partial sums and an explicit logical bridge for cross-half reads. A deliberate **negative control** proves that treating the second 32-lane subgroup independently gives an incorrect prefix at lane 32.

**Limitations:** This is *CPU logic only*. It does not execute actual HLSL, DXIL, SPIR-V, Vulkan, AMD RDNA2 instructions, WARP, NVIDIA or Intel GPU code. No claim of full wave64 GPU semantics, shader translator correctness or game render compatibility is justified by these tests. Correct GPU barriers, uniformity/divergence, memory visibility and subgroup scheduling must be validated separately.

Reference specifications: [DirectX HLSL WaveReadLaneAt](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/wavereadlaneat), [WaveGetLaneCount](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/wavegetlanecount). Relevant reported community problems: [Leonida #94](https://github.com/ItsNotRexed/Leonida/issues/94), [Xbox2Windows #145](https://github.com/R0Y00/Xbox2Windows/issues/145).

Acceptance: standard Linux GCC and Windows MSVC CI must compile and execute deterministic CPU tests with no game inputs.
