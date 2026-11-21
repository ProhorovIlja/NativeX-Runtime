# Native desktop D3D12 raster readback — synthetic Windows WARP gate

This fixture advances beyond queue/fence creation and compute-only tests. The original C++17 Windows executable compiles **its own** HLSL vertex and pixel shaders, creates a D3D12 graphics pipeline, clears an offscreen 16×16 RGBA8 texture, rasterizes one triangle, transitions the texture for copying, reads the GPU result into CPU-visible memory and independently checks a colored center pixel and untouched background.

Build: `cmake -S . -B build -A x64 && cmake --build build --config Release`; test: `ctest --test-dir build -C Release -R d3d12_warp_raster_readback --output-on-failure`.

The fixture targets the Microsoft WARP software D3D12 device so a standard free public GitHub Windows runner can reproduce it without a real GPU. A genuinely unavailable device skips (exit 77); failure to compile shaders, submit commands, synchronize or produce expected pixels fails the test. MSVC warnings remain errors.

**Limits:** this is desktop D3D12, not Xbox D3D12.X; no console RDNA2 instruction translation, desktop window/swapchain Present, physical AMD/NVIDIA/Intel GPU execution, game model, game assets or GTA VI compatibility. Successful raster readback does not demonstrate a playable frame.

Next: windowed DXGI swapchain Present using source-built synthetic frame contents; repeat on physical GPUs; establish an independent native Vulkan raster test for Linux, and only later bridge verified console command semantics.
