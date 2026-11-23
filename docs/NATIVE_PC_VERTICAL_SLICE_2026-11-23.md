# Native PC vertical slice — 2026-11-23

## Purpose

Create a **real source-built Windows x64 executable** (and Linux ELF fixture)
that exercises a continuous PC component path on **synthetic** data rather than
claiming GTA VI can boot. Target: `native_pc_vertical_slice` (on Windows,
`native_pc_vertical_slice.exe`). This is **not** `GTA6.exe`, a game launcher,
an emulator, or an executable reconstructed from Xbox game code.

## Scenario and acceptance

1. Write one known 4×4 BC1 compressed block to an actual temporary file.
2. Read bytes through the native prioritized asynchronous Reader, then publish
   a complete CPU RGBA8 texture through the existing all-or-nothing decoder.
3. Capture a real in-process, device-scoped synthetic keyboard event in the
   existing native Router; resolve the input using the actual remappable action map.
4. Serialize the map plus a representative texture pixel; commit to the native
   two-slot save implementation, reopen, load, and verify it.
5. Mix a small stereo PCM stream using the native reference mixer.
6. Corrupt the compressed data after reading, require publication to fail
   without exposing RGBA pixels, and ensure the persisted state stays unchanged.
7. Release the synthetic key and verify the restored map sees no held action.

The Windows and Linux CTest entry is
`native_pc_vertical_slice_io_input_audio_save`. CI must compile this fixture
under the repository's strict compiler flags. It exits nonzero if any stage
fails; its JSON success message explicitly reports
`"gta_vi_booted":false` and `"gpu_present":false`.

## Intentional limits

- No GTA VI EXE, game archives, copyrighted game resources or legal claims.
- The test fixture does not execute Xbox code or load RAGE 9.
- The input event is an injected per-device test record, **not** a physical
  mouse, keyboard or controller measurement and not a GTA action mapping.
- The texture is a simple linear BC1 block, **not** BCPack, RPF9 or a tiled
  Xbox GPU surface. CPU decoding is not GPU upload or D3D12.X translation.
- The PCM mix uses a synthetic float buffer and does not drive WASAPI.
- Saving proves transaction mechanics only; it is not GTA VI autosave,
  mission consistency or the proprietary save format.
- A checksum catches accidental handoff corruption; it is **not** cryptographic
  authentication, anti-malware protection or proof an asset is trusted.
- The output is useful as a regression gate for independently developed PC
  subsystems, not a measure of actual title performance.

## Native product boundary

The target remains a standalone Windows x64 `GTA6.exe` executing the actual
game. Existing GDK/graphics/PE research must be integrated or statically
adapted only where compatible with the eventual lawful title binaries.
A launcher is optional. This fixture does not relax the no-emulator shipping
contract described in
[NATIVE_WINDOWS_EXE_PRODUCT_CONTRACT_2026-11-23.md](NATIVE_WINDOWS_EXE_PRODUCT_CONTRACT_2026-11-23.md).

Only the repository's existing, on-demand PR validation is used. No new
scheduled searches, monitoring, recurring jobs, desktop services or notifications.
