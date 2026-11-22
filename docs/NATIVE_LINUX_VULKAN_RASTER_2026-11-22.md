# Native Linux Vulkan offscreen graphics raster and pixel readback

Source-built independent native x86-64 Linux ELF acceptance fixture, created 2026-11-22. The original `raster.vert` and `raster.frag` GLSL shaders are compiled by the public Ubuntu `glslangValidator` to SPIR-V during CMake builds. No generated shader binary is checked into Git, no Xbox game content is used.

The C++17 Vulkan fixture creates a real Vulkan instance and graphics-capable logical device/queue, RGBA8 optimal-tiled color attachment image, render pass, framebuffer, vertex+fragment shader modules, graphics pipeline, command pool and buffer. It submits `vkCmdDraw(3,1,0,0)` for a triangle, transfers the result to a separate **host-visible coherent CPU readback buffer**, synchronizes a render-pass-to-transfer dependency and transfer-to-host barrier, then waits on a Vulkan submission fence.

**Fail-closed image oracle:** for the 32×32 image, the center must be approximately RGBA (64,128,255,255); opposite corners must be opaque black. Every single pixel must be either triangle color or background color within a 2/255 rounding tolerance, with at least 200 pixels of each. A wrong color, missing triangle, Vulkan error or timeout fails the test; unavailable Vulkan device/graphics queue/host-visible memory returns an explicit CTest **SKIP 77**.

On public GitHub Ubuntu CI, Mesa software Vulkan (often lavapipe) can execute actual Vulkan commands through a native ELF process. That is **not physical GPU proof**, not real window/swapchain Present, not a speed/latency benchmark and not Xbox D3D12.X shader translation. The existing Windows D3D12 WARP triangle and DXGI swapchain tests independently exercise analogous desktop concepts, but the two implementations do **not** yet share a production renderer nor use the same GPU hardware.

Local reproduction:

```bash
sudo apt install libvulkan-dev libvulkan1 mesa-vulkan-drivers glslang-tools cmake g++
cmake -S fixtures/native-linux-vulkan -B build-native-vk -DCMAKE_BUILD_TYPE=Release
cmake --build build-native-vk -j2
ctest --test-dir build-native-vk --output-on-failure -V
```

Technical references: [Khronos Vulkan render passes](https://docs.vulkan.org/tutorial/latest/07_Advanced_Topics/03_Multisampling.html), [Khronos Vulkan synchronization examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html), and [Vulkan framebuffer/pipeline documentation](https://docs.vulkan.org/spec/latest/chapters/renderpass.html). No external source files are imported, modified or redistributed.

What remains after this acceptance: actual display presentation/WSI, hardware GPU (AMD/NVIDIA/Intel), image format/colorspace/HDR, frame pacing, resize/device-loss, physical GPU/VRAM memory budgets, real asset/shader conversion, then measured native upscalers and latency APIs. The Windows game port and first playable single-player scene remain the primary objective; Linux is the next native target, not a claim that Proton is an ELF port.
