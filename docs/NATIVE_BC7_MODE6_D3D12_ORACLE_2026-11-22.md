# Native BC7 BPTC mode 6: partial decoder and real D3D12 WARP differential — 2026-11-22

This is **partial BC7 support**, deliberately exposed as `Format::Bc7Mode6`, not as a generic `Format::Bc7`. A block encoded in any other BC7 mode (0–5, 7, or reserved zero mode) produces `Status::UnsupportedBlockMode`, with **no partial output** even if an unsupported block occurs after several valid blocks in a multi-block texture. Do not silently substitute visible color, uncompressed bytes, or claim a retail texture was decoded.

The original C++17 portable `native_bc_texture` module keeps BC1/BC3/BC4/BC5 unchanged. Mode 6 is one 128-bit, single-subset RGBA block: 7 unary mode bits, channel-major R0/R1, G0/G1, B0/B1, A0/A1 7-bit fields, two independent shared per-endpoint P-bits, and 16 4-bit indices with the first (fix-up anchor) stored in 3 bits. Each endpoint component is `(raw7<<1)|pbit`, and RGBA is interpolated with BC7's fixed 16-entry 0–64 weight table and integer round-to-nearest.

Sources: [Microsoft BC7 format specification](https://learn.microsoft.com/en-us/windows/win32/direct3d11/bc7-format), [BC7 mode reference](https://learn.microsoft.com/en-us/windows/uwp/graphics-concepts/bc7-format), and [Khronos Data Format BPTC](https://github.com/KhronosGroup/DataFormat/blob/main/bptc.txt). Numeric decoding is original test code derived from the **public format specification**, not copied from a third-party GPL decoder.

The CPU test suite now has **ten** scenarios, including two mode6 blocks whose 128 bits were independently encoded and selected texels have hardcoded RGBA oracles. The synthetic 5×7 case spans four compressed blocks with padded input/output rows. It also tests that an invalid/reserved/unsupported BC7 mode in the **second block** rejects the entire request without touching the output; truncated image buffers and invalid strides must also fail before writing.

Windows D3D12 WARP adds a **separate BC7_UNORM GPU/CPU differential**: a source-built VS/PS samples a real 4×4 compressed BC7 mode6 GPU texture with `Texture2D.Load`, renders to a 16×16 RGBA8 target, reads back 256 pixels/1024 channels and compares to the portable CPU reference with ≤2/255 tolerance. It may skip with exit code 77 only when WARP/DXGI or compressed texture shader-load support is unavailable, never for rendering or pixel mismatches. Preserve existing BC1/3/4/5 tests.

**Limitations:** six other BC7 modes, mode7, signed/float BC6H, full BC7 texture archive coverage, sRGB transfer handling, GPU physical-adapter performance, Xbox GPU tiling/swizzle, D3D12.X/XS command submission, and GTA VI assets remain unimplemented or unverified. This is a useful **one-mode format-specific acceptance**; do not use it as a general retail texture fallback.

No game executable, proprietary assets, new third-party decoder source, scheduled background checks, or vendor binary introduced.
