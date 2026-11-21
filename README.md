# Scarlett Windows Compat

**Open-source, Windows-native Xbox Series X|S interoperability research. Not a finished emulator or a playable commercial-game port.**

Scarlett Windows Compat develops small, reproducible **clean-room test tools** for understanding how Xbox Series Game Core/GDK software interacts with desktop Windows x64. On a PC, compatible x86-64 instructions can execute directly; Xbox-specific services, memory and GPU semantics still require careful adaptation. The research is **native-first** rather than CPU-instruction emulation.

## What is here

- `src/pe_intake.cpp` — original read-only PE32+ import/section, delayed-import, TLS-directory and relocation-block inspection; no code execution.
- `src/platform_probe.cpp` — Windows-only probe of system graphics/media/input API availability.
- `tools/radar.py` — read-only GitHub revision scanner against [pinned SHAs](sources/upstreams.json).
- `tools/discover.py` — search for relevant new public projects/forks; candidates remain unreviewed.
- `tools/commit_watch.py` — bounded metadata-only scan of recent commit subjects, never proof of a fixed bug.
- `fixtures/wave64_cpu_oracle.cpp` — source-built logical wave64 vs 2×wave32 CPU differential tests.
- `fixtures/d3d12_warp_smoke.cpp`, `fixtures/d3d12_warp_compute.cpp` — native Windows D3D12 WARP command/fence and real synthetic compute dispatch/readback tests.
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

See [project limitations](docs/STATE.md), [safe research rules](SECURITY.md) and [contribution guidelines](CONTRIBUTING.md).