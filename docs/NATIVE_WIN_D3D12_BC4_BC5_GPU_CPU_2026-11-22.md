# Windows D3D12 BC4/BC5 GPU/CPU differential (2026-11-22)

Extends the original source-built `fixtures/d3d12_warp_bc_texture.cpp` to run four formats: BC1, BC3, **BC4_UNORM**, **BC5_UNORM**. The original BC1/BC3 tests remain unchanged in CMake. No new graphics library dependency or proprietary shader code.

## Expected execution gate

On real Windows x64 desktop D3D12 with Microsoft WARP, the test creates a compressed 4×4 texture, uploads the original compressed bytes to a GPU resource via `CopyTextureRegion`, creates a shader-visible SRV, compiles a small original HLSL VS/PS, draws a triangle to a 16×16 RGBA8 target, copies its pixels back to CPU and compares **256 pixels / 1,024 RGBA channels per format** against the portable CPU decoder.

The BC4 fixture samples red-only UNORM with independent 3-bit selector palette values 0/1/2/7. The BC5 fixture uses separate endpoint orderings for red and green, exercising the 8-entry and 6-entry interpolation branches independently. Output channel mapping is R-only for BC4 or RG for BC5, with B=0 and alpha=255. The GPU-to-CPU channel comparison allows 2/255 per channel for documented texture-format interpolation rounding differences.

The test **must SKIP (exit 77)** if DXGI/WARP/D3D12 or required compressed format `TEXTURE2D | SHADER_LOAD` capabilities are absent. Such skip is not a passing GPU decode. API creation, draw, fence, and readback errors fail, not skip. All four format runs are separate Windows CTests. The Linux portable CPU decoder tests remain in the regular Linux suite.

This is a desktop **D3D12 WARP** format-sampling validation, not proof of physical GPU performance, Xbox `d3d12_xs` command translation, GFX10 swizzle, BC4/5 signed variants, BC7, or any texture in GTA VI. Record CI outcomes for the exact PR HEAD before claiming success.

No automatic monitoring, scheduled tasks, new vendored binaries or commercial assets introduced.
