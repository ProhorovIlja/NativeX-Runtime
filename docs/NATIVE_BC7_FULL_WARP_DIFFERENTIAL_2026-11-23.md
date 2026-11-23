# BC7 all-eight-mode D3D12 WARP differential — 2026-11-23

The preceding PR #115 incorporated the MIT-pinned CPU reference for all
eight legal BC7 modes. This PR extends the existing **real** native Windows
D3D12 WARP texture-upload/shader-readback fixture to exercise every mode.

## Tests

The source-built 4x4 BC7 block for each unary-prefix mode 0..7 is decoded by
the pinned CPU reference and uploaded as a compressed DXGI_FORMAT_BC7_UNORM
texture to Microsoft WARP. The real D3D12 graphics path uses a shader Load,
16x16 raster target, GPU queue/fence and readback to verify 256 output pixels
(1024 individual RGBA channels) for each mode, within the existing 2-unit
channel tolerance. The original BC7 mode6 independently generated sample and
the BC1/BC3/BC4/BC5 tests are preserved, and explicit capability skips remain.
CTest entries are `native_win_d3d12_bc7_mode{0..7}_gpu_cpu_differential`.

## Limits

These eight synthesized compressed records are **not** an exhaustive BC7
bitstream conformance test. D3D12 WARP is software-based and does not measure
RTX 4070, AMD or Intel device correctness or throughput; neither real
GTA VI assets nor Xbox tiled/swizzled surfaces, BCPack, RPF9, title shader,
XGDK commands, a booted game or FPS are tested. Driver and real GPU follow-up
must use independently validated test vectors, not this one fixture.

No recurring tasks, notifications or background monitors were added.
