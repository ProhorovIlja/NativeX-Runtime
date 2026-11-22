# Native D3D12 WARP BC1/BC3 GPU texture sampling with independent CPU reference

Original Win32/MSVC x64 graphics fixture `fixtures/d3d12_warp_bc_texture.cpp`, created 2026-11-22. This test is a step beyond pure BCn software parsing and an untextured WARP triangle. It **actually creates GPU BC1/BC3 compressed textures, uploads them, binds SRVs, executes a pixel shader that reads the compressed texture, rasterizes a fullscreen image, and reads back the GPU output for byte-level comparison**.

## End-to-end source-built chain

1. Independently author one 4×4 BC1 block (RGB565 red/blue palette and all four packed 2-bit selector values) and one 4×4 BC3 block (RGB565 black/white four-color palette and 0/1/2/7 packed 3-bit alpha selectors). The previously independently tested [native BC1/BC3 CPU decoder](NATIVE_BC1_BC3_TEXTURE_2026-11-22.md) produces each 4×4 RGBA reference image.
2. Create a genuine Windows **D3D12 WARP** device and query `D3D12_FEATURE_FORMAT_SUPPORT` for BC1/BC3 `TEXTURE2D | SHADER_LOAD` capability. If unsupported, report a **CTest SKIP 77**, never claim GPU correctness.
3. Create a genuine `DXGI_FORMAT_BC1_UNORM` / `BC3_UNORM` **4×4 compressed GPU texture**, `GetCopyableFootprints`-aligned upload buffer and real GPU `CopyTextureRegion`; transition from `COPY_DEST` to `PIXEL_SHADER_RESOURCE`.
4. Create a shader-visible **D3D12 SRV descriptor heap**, explicit pixel-visible root descriptor table `t0`, a root signature and compiled original HLSL vertex/pixel shaders. The fragment shader uses `Texture2D<float4>.Load` to read BC-compressed texels from the GPU. Draw a fullscreen triangle into a separate **16×16 RGBA8** render target, scaling each 4×4 source texel to a 4×4 destination square without filtering.
5. Transition render target into `COPY_SOURCE`, GPU-copy all 16×16 pixels into a proper aligned host readback buffer, signal and await a D3D12 fence. Compare **256 rendered pixels and all 1024 RGBA channels** to the CPU decoder's known values, permitting at most two integer 8-bit units for allowed hardware interpolation rounding. Any unexpected color, alpha, missing shader binding, device removal or fence timeout fails.

Run automatically as *two independent CTests* on Windows x64 in existing CI: `native_win_d3d12_bc1_gpu_cpu_differential` and `native_win_d3d12_bc3_gpu_cpu_differential`. The original BC1/BC3 CPU fixture remains a portable Linux ELF test too. No new schedule or automated monitoring was created.

## Limits

This validates a **true D3D12 texture resource, SRV binding and GPU-side BC block decode**, not merely `CreateDevice`. GitHub's Windows runner uses software WARP; physical AMD/NVIDIA/Intel adapters, real VRAM residency, BC7/BC6H hardware decoding, variable texture arrays/mips and console-specific tile/swizzle layout are not verified. The 4×4 compressed blocks are artificially authored, **not** derived from GTA VI or another proprietary title.

The test uses HLSL Shader Model 5.0 desktop graphics, **not Xbox GDK D3D12.X command lists or translated RDNA2 ISA**. It does not render a Rockstar material or a playable frame and gives no FPS or performance claim. Future compatibility requires the exact memory layout and shader/resource bindings of a legitimately available Xbox title.

Source references: [Microsoft block compression formats](https://learn.microsoft.com/en-us/windows/desktop/direct3d10/d3d10-graphics-programming-guide-resources-block-compression), [D3D12 format support flags](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_format_support1), [D3D12 resource descriptor tables](https://learn.microsoft.com/en-us/windows/win32/direct3d12/creating-a-root-signature).
