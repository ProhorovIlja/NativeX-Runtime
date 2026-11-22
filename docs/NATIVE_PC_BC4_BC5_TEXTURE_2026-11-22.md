# Portable BC4/BC5 UNORM CPU texture reference — Windows/Linux

2026-11-22 extension to the already tested `src/native_bc_texture.{hpp,cpp}` and `fixtures/native_bc_texture_tests.cpp`, preserving the existing BC1/BC3 public decoder API. No shader code, proprietary game resources or third-party algorithms were copied.

## Additional decoded texture formats

- `Bc4`: one independent BC4/DXT5-style 8-byte channel block per 4×4 texels. The decoded scalar is written to R; G and B become zero, alpha is opaque.
- `Bc5`: two consecutive independent BC4 8-byte blocks per 4×4 texels. The first stores R, the second stores G; B=0 and alpha=255. This is useful for two-channel data such as XY components of normal maps, without claiming the game's normal-map encoding or reconstructing Z.

Both blocks use the full 48-bit little-endian 3-bit selector stream, distinct endpoint interpolation branches (`endpoint0 > endpoint1` with six interpolated entries versus the lower-or-equal branch with four interpolation entries and 0/255 sentinels). The decoder continues to validate visible dimensions (max 8192), padded compressed row pitch, full input span, destination row pitch, and output span **before any writes**, including rectangular images not divisible by four.

Existing `Bc1` and `Bc3` behavior is unchanged; the common BC3 alpha table and six-byte selector parser are also used by BC4 and BC5. Output is always conventional tightly addressed RGBA8 texels with optional padded destination stride.

## Differential/negative fixture coverage

The extended test covers both BC4 interpolation branches using explicit first-pixel selector oracles, two independent BC5 R/G palettes, a 5×7 image across four 4×4 BC5 blocks with both source and destination row padding, invalid compressed pitch, insufficient source bytes, invalid output pitch, and assertion of **no output mutation** for invalid inputs. Existing five BC1/BC3 scenarios remain intact; the suite now runs eight scenarios.

**Important limits:** These are `UNORM` formats, **not BC4_SNORM/BC5_SNORM**; BC6H/BC7 are still unsupported, as are retail Xbox/GFX10 swizzle/tile mapping and game asset formats. A future independent GPU test should upload known BC4/BC5 blocks into native D3D12 and Vulkan and compare sampled output with this CPU oracle, allowing documented rounding tolerance. Physical GPUs, normal-map presentation, and retail GTA VI textures are untested.

No workflow schedule, background monitor, new external dependency, or vendor blob was added.
