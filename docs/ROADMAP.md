# Milestones and acceptance gates

## M0 — public, no protected data
- Pin baseline SHAs and licensing metadata for multiple upstreams.
- Add repeatable Python source-radar and candidate search (read-only metadata).
- Compile C++17 PE32+ inspector and Windows host prerequisite checker (**passing on standard public CI**).
- Test native Windows named/ordinal DLL exports (**passing on standard public CI**).
- Run deterministic Linux and Windows CI tests on synthetic fixtures.
- Record upstream issues and reproducibility blockers with direct links.
- Independent pinned public-source Windows x64 CMake/MSVC build of the AnyXboxX launcher and five shim DLLs **passed**; actual open-source GDK sample execution and console D3D12.X correctness still pending.

## M1 — generic Windows compatibility experiments
- PE32+ source-built metadata inventory: ordinary/delayed imports, TLS presence, relocs, exports, ordinals and forwarders (**passing**); **actual TLS callback/unwind loader behavior is still pending**.
- Win32 virtual memory and logical 64 KiB page remapping/dirty staging model (**passing**); true Xbox physical remapping and GPU coherency pending.
- Source-built HLSL Shader Model 6 wave ops executed on D3D12 WARP with CPU readback (**passing**); AMD/NVIDIA/Intel hardware, native RDNA2 ISA and Shader Model 6.6 compatibility pending.
- Optional physical AMD/NVIDIA/Intel adapter selection and SM6 differential **implemented**; no suitable GPU on public runner, correctly SKIPPED, hardware acceptance still pending.
- Synthetic MSVC x64 COM vtable/16-byte hidden struct-return **passing**; title-specific Xbox D3D12.X method ABI and record format still unknown.
- D3D12.X-to-D3D12 interface mapping with source-built stand-ins; synthetic command-record writer/parser bounds tests added (NOT actual Xbox formats); prioritize inline console command-record layout and DMA lists, not generic PC DLL forwarding.
- GPU wave32/64, barriers, descriptors, RDNA2 opcode correctness on synthetic workloads.
- MSVC/Clang cross-checks and Windows 10/11 compatibility.

## M2 — game-specific work only with lawful and complete inputs
- Version-pinned, read-only asset and EXE inventory performed locally (never CI uploads).
- Record genuine ABI imports and missing services; no DRM bypass.
- Reproduce first interactive frames, audio/input and complete storage behavior.

## M3 — quality and performance
- Fix game correctness and stability, save reliability and crash handling.
- Measure CPU/GPU/VRAM usage and frame pacing on defined hardware.
- Future optional multiplayer/co-op only when core experience is stable.

Milestones are not percentages of an unavailable game.