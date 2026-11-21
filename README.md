# Scarlett Windows Compat

**Open-source, Windows-native Xbox Series X|S interoperability research. Not a finished emulator or a playable commercial-game port.**

Scarlett Windows Compat develops small, reproducible **clean-room test tools** for understanding how Xbox Series Game Core/GDK software interacts with desktop Windows x64. On a PC, compatible x86-64 instructions can execute directly; Xbox-specific services, memory and GPU semantics still require careful adaptation. The research is **native-first** rather than CPU-instruction emulation.

## What is here

- `src/pe_intake.cpp` — original read-only PE32+ import/section, delayed-import, TLS-directory and relocation-block inspection; no code execution.
- `src/platform_probe.cpp` — Windows-only probe of system graphics/media/input API availability.
- `tools/radar.py` — read-only GitHub revision scanner against [pinned SHAs](sources/upstreams.json).
- `tools/discover.py` — search for relevant new public projects/forks; candidates remain unreviewed.
- `tools/commit_watch.py` — bounded metadata-only scan of recent commit subjects, never proof of a fixed bug.
- `tools/sdk_radar.py` — official PC graphics, Windows DirectStorage, Proton and vkd3d-proton release metadata; GitHub tags are not runtime capability evidence (see [SDK radar](docs/SDK_RELEASE_RADAR.md)).
- `sources/input_upstreams.json` — separate pinned GameInput, SDL3, HIDAPI and DualSense-reference commit monitor ([input stack review](docs/INPUT_UPSTREAM_REVIEW_2026-11-22.md)); no unlicensed source or virtual controller drivers imported.
- `fixtures/wave64_cpu_oracle.cpp` — source-built logical wave64 vs 2×wave32 CPU differential tests.
- `fixtures/d3d12_warp_smoke.cpp`, `fixtures/d3d12_warp_compute.cpp` — native Windows D3D12 WARP command/fence and synthetic SM5 compute readback.
- `fixtures/wave_lanes_sm6.hlsl`, `fixtures/d3d12_wave_sm6.cpp` — source-built Shader Model 6 wave intrinsics **actually executed on WARP** and verified against CPU output.
- `fixtures/win_memory_alias.cpp`, `fixtures/physical_page_cpu_model.cpp` — Win32 memory and independent page-identity/remapping model.
- `fixtures/msvc_struct_return_abi.cpp` — native Windows x64 synthetic virtual struct-return ABI regression test.
- `fixtures/command_record_cpu_model.cpp` — bounded synthetic graphics/DMA stream writer/parser with corruption and memory-safety guards (not real Xbox command records).
- `tools/verify_anyxboxx_native.ps1` — exact-SHA independent external source build of an LGPL upstream native Windows launcher; does not execute it or copy binaries into this repository.
- The SM6 fixture has distinct `warp` and vendor-checked `hardware` modes; the public runner passed WARP and correctly skipped unavailable AMD/NVIDIA/Intel hardware.
- `src/pe_intake.cpp` also reports PE32+ named/ordinal exports and forwarded-export metadata with bounded tables, including checks against our own compiled DLL.
- `docs/UPSTREAM_REVIEW_2026-11-22.md` — dated comparison of changes in seven upstream projects, with links and explicit proof gaps.
- `docs/ARCHITECTURE.md`, `docs/SOURCES.md` and `docs/ROADMAP.md` — engineering baselines and evidence discipline.
- `.github/workflows/ci.yml` — synthetic Linux/Windows tests on standard **public-repository** runners.

Nothing here runs a retail Xbox title. Repository percentages and screenshots from third parties are **not** evidence of native game execution. We neither host nor request copyrighted game EXEs, proprietary DLLs, firmware, game assets, keys or leaked SDKs.

## Build and test

Python 3.11+, CMake 3.20+, C++17 compiler:

```shell
python -m unittest discover -s tests -v
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
python tools/radar.py --offline-fixture tests/fixtures/public_metadata.json
python tools/discover.py --offline-fixture tests/fixtures/discovery.json
```

On Windows x64, `win_native_probe.exe` prints **host capabilities**, not Xbox compatibility. `pe_intake` inspects a local file **without executing it**. Linux CI tests use synthetic files and intentionally never run unknown executables.

## Engineering and licensing

All project's original source files are published under MIT (see LICENSE). Upstream projects have **different** licenses (including GPL and LGPL) and are **references**, not embedded dependencies. No third-party source code is copied without an explicit license, attribution and compatibility review.

See [the native Windows end-state definition](docs/NATIVE_TARGET.md), [project limitations](docs/STATE.md), [safe research rules](SECURITY.md) and [contribution guidelines](CONTRIBUTING.md).
Research updates: [2026-11-22 upstream ABI/shader delta](docs/UPSTREAM_DELTA_2026-11-22.md). Exact upstream author claims and independently executed tests are recorded separately.

Current watched branch-tip snapshot: [2026-11-22 upstream checkpoint](docs/UPSTREAM_CHECKPOINT_2026-11-22.md). Original SHA baselines retained separately; commit movement is not proof of game compatibility.

Native upstream-source reproduction: [AnyXboxX clean-build gate](docs/ANYXBOXX_NATIVE_BUILD_GATE.md) and [independent Windows run](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44956507116). This confirms a public-source build only, not gameplay.

Public research inventory now tracks 19 source repositories, including Microsoft PC GDK samples, DXC, LLVM, DirectX Graphics Samples and shadPS4 as a distinct architecture-only reference: [additional primary references](docs/PRIMARY_PUBLIC_REFERENCES_2026-11-22.md).

Actual independent native PE32+ execution through the pinned upstream (three original CRT-free test programs returned expected exit codes **42 / 43 / 44**): [verified Windows CI](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44957418968) and [fixture sources](fixtures/native-pe-exec). This is not a GDK title or commercial-game boot.

Native-loaded desktop D3D12 smoke: [PR #27](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/27) and [CI result](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44958262954), validating WARP device, queue and fence from the original PE's mapped entrypoint; **no Xbox D3D12.X rendering yet**.


Future native Windows + Linux graphics/IO/audio/latency work, optional FSR/DLSS/XeSS, Reflex/Anti-Lag2/XeLL, DLSS 5 neural research, DirectStorage and native io_uring: [technology architecture](docs/NATIVE_PC_TECH_STACK.md). Main contains a source-built and CI-tested fail-closed cross-platform feature policy and a host library inventory; **no vendor SDK integration is yet claimed**.

Actual portable Windows/Linux asynchronous asset reads (native threads, cancellation, bounded chunks and hash checks) are source-built and CI-tested: [PR #29](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/29), [technical scope](docs/NATIVE_ASSET_STREAMING.md). This is a portable baseline, not DirectStorage or io_uring yet.

Recent verified source-built PC work:
- [Portable native Windows/Linux asset streaming](docs/NATIVE_ASSET_STREAMING.md) ([PR #29](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/29)); not DirectStorage or io_uring.
- [Renderer-specific modern SDK feature guards](docs/GPU_API_INTEGRATION_GATES_2026-11-22.md) ([PR #35](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/35)); 14 policy scenarios, no vendor effects claimed.
- [Native desktop D3D12 WARP triangle raster + verified pixel readback](docs/D3D12_RASTER_READBACK_2026-11-22.md) ([PR #36](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/36)); 18/18 Windows tests pass, but no Xbox D3D12.X or GTA VI game frame.
