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

## Native Linux Vulkan partition test

`native_linux_vulkan_wave64_partition` runs `fixtures/native_wave64_partition.comp` with 64 invocations per workgroup. The shader exchanges data through workgroup shared memory and barriers. The CPU oracle in `tools/wave64_partition_vectors.py` supplies the inputs and expected results for seed `0x57415645`. It checks partition masks, sums and guarded lane reads across the 31/32 boundary, including inactive and out-of-range source lanes.

With a Vulkan 1.1 driver, Vulkan development headers, CMake, a C++17 compiler, Python 3, `glslangValidator` and `spirv-val`, run from the repository root:

```sh
cmake -S fixtures/native-linux-vulkan -B build-vk -DCMAKE_BUILD_TYPE=Release
cmake --build build-vk --target linux_vulkan_wave64_partition
ctest --test-dir build-vk -R '^native_linux_vulkan_wave64_partition$' --no-tests=error -V
```

The build compiles and validates the shader before CTest dispatches it. To compare drivers, repeat the CTest command with `VK_DRIVER_FILES` set to each installed ICD manifest in turn. A pass covers every output word and reports the driver's default subgroup size. A mismatch reports the first failing workgroup, logical lane, field index, expected value and GPU value. Native subgroup shuffle instructions and AMD wave64 execution require separate tests.
