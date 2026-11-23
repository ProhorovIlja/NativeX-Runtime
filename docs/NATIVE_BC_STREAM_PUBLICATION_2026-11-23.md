# Native asset-reader → BCn RGBA8 publication — 2026-11-23

This original C++17 Windows/Linux component connects the existing **real native OS file reader** to the existing **CPU reference BC texture decoder**. Its purpose is to establish a correct, bounded, reusable resource handoff before any GTA VI executable is available.

**Not implemented:** RPF9 encrypted TOC handling, BCPack decompression, AMD swizzled image layout, actual GPU uploads, real shader binding, Xbox GDK runtime or GTA VI game startup. The synthetic input is linear BC1/BC3/BC4/BC5/BC7 mode 6 data; it is not a GTA VI texture. The code is not an FPS benchmark.

## Source-built API

- [Publisher header](../src/native_asset_texture_publish.hpp) and [implementation](../src/native_asset_texture_publish.cpp).
- Caller submits a bounded native `stream::Request` with selected `Priority` and obtains a `stream::Result` from `Reader`. It then explicitly calls `texturestream::publish(source, format, width, height, compressed_block_row_bytes)` on its own thread. This avoids spawning a thread per decode. The publisher does not manage global queues.
- `Ready` returns a complete row-major RGBA8 image. Any other status returns an **empty pixel vector**, not an incomplete texture. In particular, a failed/truncated/cancelled read, incorrect compressed length, inconsistent Reader FNV-1a checksum, invalid format/stride, unsupported BC7 block mode, dimensions outside 1–8192 or output over 32 MiB fails without publishing.
- The maximum 32 MiB output budget is intentionally conservative for this sample utility and **not** a GTA VI texture-size requirement. The compressed input to Reader is still capped at 4 MiB per request. Pitch sizes and total output are checked with overflow-safe arithmetic before allocation.
- FNV-1a detects inconsistent transfer of a source buffer between the Reader and decoder when the result is copied or inadvertently modified. **It is not a cryptographic signature or proof a user-supplied file is safe or authentic.**
- Existing texture decoder supports BC1, BC3, BC4, BC5 and **only BC7 mode 6**. Other BC7 modes are correctly rejected. No BC6H/BC7 full decoder or BCPack implementation is implied. The existing CPU decoder performs a full mode-6 preflight so no partial mode-6 surface is exposed.
- Original disk data remains read-only. No background service, auto-update, network transfer or recurring scheduled task is created.

## Portable acceptance tests

[Five scenario groups](../fixtures/native_asset_texture_publish_tests.cpp) exercise:

1. Two independent completed source-built BC1 records read via the native asynchronous Reader with FrameCritical/Normal priorities, emitted as complete 4×4 red and green RGBA8 images in the caller's chosen order. Verify all pixels and that disk bytes did not change.
2. The second record is deliberately cut in half: the first image is publishable, the second produces an invalid native range and **no** published output.
3. Incorrect checksum, unsupported format, zero/excessive dimensions, output budget, malformed compressed pitch/length, cancelled reads and the guarantee that every failure returns no pixel buffer.
4. Unsupported BC7 mode must refuse publication; valid mode-6 4×4 source-built reference must decode correctly.
5. An odd-size 5×7 BC1 image with compressed row padding tests four blocks, correct crop and exact visible texels, loaded through the Background stream priority.

The tests don't claim a GPU upload or output frame. Real game rendering validation and timing would require a verified lawful Xbox build/executable, authoritatively mapped resource data and a functioning renderer.

## Upstream relation

`R0Y00/Xbox2Windows` [PR #850](https://github.com/R0Y00/Xbox2Windows/pull/850) (23 November 2026) performs a separate author-reported native vs mapped vs AOT differential experiment involving partial archive decode and **4×4 RGBA8 texture publication**. Its motivation is similar: never expose a half-decoded resource. Our implementation reuses **only our own existing portable Reader and BCn reference decoder**; it does not import upstream source or claim the same Zig/PE/AOT behavior.

## Next tests on a real game

When a verified, legally usable build becomes accessible, first determine Xbox resource unpack/swizzle/mip rules and the GPU's expected pitches. Follow with strict format-specific decode and per-resource upload validation, proper GPU descriptor bindings, mip residency and upload-fence lifetime. Only then evaluate shader/color matches and frame-time impact. Consider DirectStorage after measuring actual I/O stalls; this portable Reader is not DirectStorage.
