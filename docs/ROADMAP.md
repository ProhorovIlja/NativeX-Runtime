# Milestones and acceptance gates

## M0 — public, no protected data
- Pin baseline SHAs and licensing metadata for multiple upstreams.
- Add repeatable Python source-radar and candidate search (read-only metadata).
- Compile C++17 PE32+ inspector and Windows host prerequisite checker.
- Run deterministic Linux and Windows CI tests on synthetic fixtures.
- Record upstream issues and reproducibility blockers with direct links.

## M1 — generic Windows compatibility experiments
- Synthetic PE32+ loader tests: TLS, imports, delay-load, relocations, unwinding.
- D3D12.X-to-D3D12 interface mapping with source-built stand-ins.
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