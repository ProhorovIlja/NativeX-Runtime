# Native BC1/BC3 linear 4×4 block texture decoding — Windows/Linux

Original source-built portable C++17 files `src/native_bc_texture.{hpp,cpp}` and `fixtures/native_bc_texture_tests.cpp`, created 2026-11-22. Unlike the earlier NV12 video-surface conversion, this does **decompress original block-compressed texture bytes** into a caller-owned RGBA8 buffer, using no external library, heap allocation inside the decode function, proprietary content or GDK service.

## Exact implemented formats

**BC1 / DXT1**: each 4×4 block occupies **8 bytes**. The decoder reads little-endian RGB565 endpoints, expands 5/6-bit channels to 8 bits, extracts 16 packed 2-bit palette selectors, applies four-color interpolation when `endpoint0 > endpoint1` and the two-color/transparent-index mode otherwise.

**BC3 / DXT5**: each 4×4 block occupies **16 bytes**. The first 8 bytes contain two alpha endpoints and 16 packed 3-bit alpha indices. The decoder implements both eight-level interpolation (`alpha0 > alpha1`) and six-level interpolation with explicit 0/255 indices (`alpha0 <= alpha1`). Its following BC1-like RGB color endpoints are always interpreted using four-color interpolation, independent of their order.

The data model exposes visible `width` and `height`, the **compressed row pitch in bytes**, total compressed size, destination RGBA8 row pitch and total destination size. Rectangles need not be divisible by 4 in this standalone CPU surface representation: only visible edge texels are written, with `ceil(width/4)` blocks per row. The independent 5×7 fixture uses a two-column, two-row block raster with intentionally **non-power-of-two stride** and four different colors to catch accidental bitmasking of a non-power-of-two block-grid width. Output row padding remains unchanged.

The core checks dimensions (maximum 8192×8192), null pointers, block pitch, destination pitch, overflow in extent arithmetic, minimum source buffer size and minimum destination buffer size **before the first output write**. The caller must provide non-overlapping source/destination buffers valid for the call and explicitly know whether compressed input is linear BC1 or BC3. A wrong format or malformed buffer returns a named error, with unchanged caller output.

## CI and test evidence

The test includes independently authored exact reference colors for BC1 red/blue/purple four-color interpolation, cutout transparent pixel, BC3 alpha 8-level and 6-level modes, 5×7 4-color rectangular block placement and strict all-byte output-canary checks on malformed inputs. Windows MSVC and Linux GCC compile with warnings as errors.

Microsoft's public format descriptions provide the external semantics, not implementation code: [Direct3D block compression overview](https://learn.microsoft.com/en-us/windows/desktop/direct3d10/d3d10-graphics-programming-guide-resources-block-compression), [BC1 bit layout](https://learn.microsoft.com/en-us/windows/uwp/graphics-concepts/opaque-and-1-bit-alpha-textures), [BC3 alpha interpolation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/textures-with-alpha-channels). [Xbox2Windows PR #399](https://github.com/R0Y00/Xbox2Windows/pull/399) independently illustrates why **odd block widths** need whole-image raster oracles, not just isolated 4×4 checks. No code from that PR is imported.

## Important restrictions

This is a **CPU fallback/reference decoder for conventional linear BC1 and BC3**, not BC7, BC6H, BC5, BCPack, GPU sampler decode, a texture uploader, an RPF archive parser or a renderer. Any real Xbox Series resource requires verified **console texture tile/swizzle/linearization**, mip and array layout, SRGB transfer, alpha mode, page ownership and actual game asset format before its compressed bytes are accepted. No GTA VI textures have been obtained, decoded or rendered. A GPU texture path should benchmark its native hardware compressed formats rather than upload all pixels as RGBA8 by default.
