# Linux native ELF Vulkan compute shader readback

2026-11-22 — an **original**, asset-free source-built Linux x86-64 ELF test independently dispatches a GLSL→SPIR-V compute shader through Vulkan and compares **all 256 computed 32-bit integers** against the CPU oracle.

## Pipeline / concrete claims

- Original GLSL compute shader `fixtures/native-linux-vulkan/readback.comp` compiles with `glslangValidator -V --target-env vulkan1.0` on the Linux CI runner; no prebuilt shader binaries or game files are downloaded.
- C++17 Vulkan test `compute_readback.cpp` creates a real Vulkan instance and logical compute device/queue, host-visible/coherent descriptor-bound storage buffer, compute pipeline, command pool/buffer, submit fence, and compute-to-host memory barrier. It dispatches 4 workgroups × 64 shader invocations and waits for the completed submission. The resulting 256 integers are checked exactly against `(index * 37 + 11) XOR 0x5a5aa5a5`.
- The bounded 1 MiB SPIR-V loader validates length/alignment and magic before shader module creation. The shader outputs overwrite a host-initialized sentinel. A mismatch or Vulkan API error fails CI. A missing capable queue/memory type produces an explicit CTest skip (77), not false success.
- CI uses free Ubuntu x86-64 runners with `libvulkan-dev`, `mesa-vulkan-drivers` and `glslang-tools`. Mesa software Vulkan (lavapipe) is an expected valid **native Linux Vulkan API execution**, not evidence of GPU hardware acceleration. The JSON distinguishes logical device and shader execution from hardware-GPU verification.

## Research sources and limitations

- [Khronos Vulkan Compute Shaders](https://docs.vulkan.org/guide/latest/compute_shaders.html) and [synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html).
- [Khronos CI render validation](https://github.khronos.org/Vulkan-Site/tutorial/latest/ML_Inference/Desktop_Applications/04_ci_render_validation.html) explicitly discusses software Vulkan in headless CI.
- [Sascha Willems computeheadless reference](https://github.com/SaschaWillems/Vulkan/tree/master/examples/computeheadless) reviewed for general stages, but **no source copied**.

This test is a native ELF Vulkan compute execution and **not** a game renderer, Xbox D3D12.X shader translator, hardware Radeon/NVIDIA/Intel GPU proof, image rasterization, HDR presentation, or an actual window/swapchain Present. Production streaming, async transfers, shader cache, frame pacing and FSR/DLSS/XeSS must be integrated and benchmarked separately on real game assets. Proton is an optional Windows compatibility route on Linux, not a substitute for this independent native ELF path.

## Local reproduction

```bash
sudo apt install libvulkan-dev libvulkan1 mesa-vulkan-drivers glslang-tools cmake g++
cmake -S fixtures/native-linux-vulkan -B build-vk -DCMAKE_BUILD_TYPE=Release
cmake --build build-vk -j2
ctest --test-dir build-vk --output-on-failure -V
```

The fixture outputs a bounded JSON summary with vendor ID, device type, compute queue, checksum, verified readback and explicit untested capabilities.
