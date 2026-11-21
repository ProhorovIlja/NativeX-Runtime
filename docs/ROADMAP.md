# Milestones and acceptance gates

## M0 — public, no protected data
- Pin baseline SHAs and licensing metadata for multiple upstreams.
- Add repeatable Python source-radar and candidate search (read-only metadata).
- Compile C++17 PE32+ inspector and Windows host prerequisite checker (**passing on standard public CI**).
- Test native Windows named/ordinal DLL exports (**passing on standard public CI**).
- Run deterministic Linux and Windows CI tests on synthetic fixtures.
- Record upstream issues and reproducibility blockers with direct links.
- Independent pinned public-source Windows x64 CMake/MSVC build of AnyXboxX launcher and five shim DLLs **passed**; followed by **3 successful original PE32+ native execution probes** (entry, Win32 imports, limited shim exports) on standard Windows CI. Actual public GDK sample rendering and Xbox D3D12.X correctness remain pending.

## M1 — generic Windows compatibility experiments
- PE32+ source-built metadata inventory: ordinary/delayed imports, TLS presence, relocs, exports, ordinals and forwarders (**passing**); **actual TLS callback/unwind loader behavior is still pending**.
- Win32 virtual memory and logical 64 KiB page remapping/dirty staging model (**passing**); true Xbox physical remapping and GPU coherency pending.
- Source-built HLSL Shader Model 6 wave ops executed on D3D12 WARP with CPU readback (**passing**); AMD/NVIDIA/Intel hardware, native RDNA2 ISA and Shader Model 6.6 compatibility pending.
- Optional physical AMD/NVIDIA/Intel adapter selection and SM6 differential **implemented**; no suitable GPU on public runner, correctly SKIPPED, hardware acceptance still pending.
- Synthetic MSVC x64 COM vtable/16-byte hidden struct-return **passing**; title-specific Xbox D3D12.X method ABI and record format still unknown.
- Native-loaded original PE32+ → desktop D3D12 WARP device/queue/fence proof **passed** on free Windows CI; rendering, Present, actual D3D12.X and hardware-specific RDNA2 pending.
- D3D12.X-to-D3D12 interface mapping with source-built stand-ins; synthetic command-record writer/parser bounds tests added (NOT actual Xbox formats); prioritize inline console command-record layout and DMA lists, not generic PC DLL forwarding.
- GPU wave32/64, barriers, descriptors, RDNA2 opcode correctness on synthetic workloads.
- MSVC/Clang cross-checks and Windows 10/11 compatibility.

## PC technology upgrades — source-built, independent of retail game inputs
- [x] Windows and Linux source-built fail-closed graphics/storage/latency selection policy + 10 deterministic cases ([PR #28](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/28)).
- [x] Windows/Linux library export inventory, explicitly not actual SDK availability.
- [x] Native Windows/Linux threaded asset reads, offsets, cancellation, checksum and shutdown tests ([PR #29](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/29)).
- [ ] Actual native window/present: D3D12 Windows, Vulkan Linux and optionally Windows ([#30](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/issues/30)).
- [ ] FSR/XeSS/DLSS SR with verified SDK and scene temporal inputs; DirectSR where available ([#31](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/issues/31)).
- [ ] Frame generation, NVIDIA Reflex, Anti-Lag 2, XeLL, permitted DLSS 5 neural, latency and image-quality profiling ([#32](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/issues/32)).
- [ ] Actual Windows DirectStorage + validated legal codec vectors and native Linux io_uring backends ([#33](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/issues/33)).
- [ ] Native Linux ELF game/program path and native Windows/Linux audio, input, saves, network, device testing ([#34](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/issues/34)).

## M2 — game-specific work only with lawful and complete inputs
- Version-pinned, read-only asset and EXE inventory performed locally (never CI uploads).
- Record genuine ABI imports and missing services; no DRM bypass.
- Reproduce first interactive frames, audio/input and complete storage behavior.

## M3 — quality and performance
- Fix game correctness and stability, save reliability and crash handling.
- Measure CPU/GPU/VRAM usage and frame pacing on defined hardware.
- Future optional multiplayer/co-op only when core experience is stable.

Milestones are not percentages of an unavailable game.