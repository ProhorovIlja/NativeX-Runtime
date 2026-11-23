# Full BC7 CPU reference integration — 2026-11-23

## Why this exists

Earlier `native_bc_texture` implemented **BC7 mode 6 only** and rejected
other modes by design. That made it impossible to publish many valid BC7
textures even when a resource was otherwise complete.

This change adds **Format::Bc7All** while preserving the previous
`Format::Bc7Mode6` as a separately testable original oracle. The new path
uses [iOrange/bcdec](https://github.com/iOrange/bcdec), pinned upstream
commit `80859ed3b7afb1c527a2a99d70c61457bea72d0c`,
`bcdec.h` 0.985. The upstream project offers MIT / Unlicense; this
repository uses the **MIT alternative** and vendors the original LICENSE.
The third-party implementation is built as an isolated native static library.

## Verified boundaries of the implementation

- Eight legal unary-prefix BC7 modes (0..7) can be dispatched through the
  complete portable CPU decoder.
- Entire-block prevalidation rejects reserved/invalid zero prefixes **before**
  any pixel is written; later invalid blocks cannot publish partial pixels.
- A 4x4 temporary block protects non-4-aligned dimensions and row padding.
- Existing strict Reader-to-texture publication validates length, dimensions,
  strides and non-cryptographic consistency before returning an RGBA8 vector.
- The original mode6 CPU path remains unchanged and offers a differential
  regression oracle for one independently generated test block.

New CTest: `native_pc_bc7_all_cpu_modes_and_atomicity`.

## Explicitly NOT validated

- No Xbox GFX10 tile/swizzle layouts, BCPack decompression, RPF9 archive
  parsing, GTA VI executable, asset images or game renderer.
- No all-mode D3D12 GPU differential yet. The existing WARP test only
  supports its own prior BC7 mode6 fixtures.
- Mode-coverage tests use synthetic blocks; passing them is not proof that
  every source-image and driver will agree. Independent hardware and golden
  GPU oracles remain future acceptance gates.
- This is a CPU fallback and a correctness tool, not proof of greater FPS or
  high GPU throughput. The decoder is not a graphics-API conversion layer.

No new scheduled processes, automation workflows, recurring checks or
monitoring were created.
