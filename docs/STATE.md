# Development state — 2026-11-22

- Public clean repository: **yes**, new independent history.
- Imported proprietary assets/executables: **none**.
- Runnable Xbox Series title or commercial game: **not demonstrated**.
- Original read-only PE32+ inspector: Linux/Windows CI passed for baseline synthetic tests. Extended import/TLS/relocation metadata and corruption rejection passed on Linux and Windows in public CI.
- Windows-native host capability probe: public Windows x64 MSVC CI passed.
- GitHub revision/discovery/commit-subject scanners: offline tests and public Linux CI passed; daily bounded API reports configured.
- Wave64 vs split-wave32 CPU differential harness: source-built 330 deterministic cases, Linux/Windows CI passed, **GPU wave correctness not claimed**.
- Desktop D3D12 WARP: native Windows device, queue/fence, Shader Model 5 compute dispatch and CPU readback passed on free Windows runner ([run 44951977570](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44951977570)); **not Xbox D3D12.X or RDNA2**.
- Source audit: [dated upstream review](UPSTREAM_REVIEW_2026-11-22.md); eight tracked branch heads changed since original baseline; no external runtime selected.
- Native Windows named/ordinal DLL loader fixture: source-built test passed in Windows CI.
- Original read-only export/forwarder parsing: synthetic corruption cases and **real source-built MSVC DLL exports** passed in Linux/Windows CI ([PR #13](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/13)).
- Win32 memory: reserve/commit/protection/decommit and two alias views passed on native Windows; independent 64 KiB logical page-remapping reference passed Linux and Windows tests ([PR #12](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/12), [PR #17](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/17)).
- Shader Model 6: **native D3D12 WARP GPU execution** of original HLSL WaveOps (`WaveReadLaneAt`, `WaveActiveSum`, `WaveGetLaneIndex`, `WaveGetLaneCount`) with CPU readback comparison passed ([PR #18](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/18), [CI run 44953548531](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44953548531)).
- Optional real-GPU SM6 differential: [PR #19](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/19) merged. **WARP passed**, AMD/NVIDIA/Intel test **SKIPPED** on public Windows runner ([run 44954553642](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44954553642)). Microsoft DXGI vendor `0x1414` was explicitly excluded after finding a misleading virtual-adapter pass.
- MSVC x64 synthetic 16-byte virtual struct-return ABI test: [PR #20](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/20) **passed**, confirming `this,hidden-result` order for the generated method in Windows CI. Not a console ABI confirmation.
- Evidence report: [2026-11-22 upstream D3D12.X and shader delta](UPSTREAM_DELTA_2026-11-22.md), including XCL's author-reported inline command-record blocker and RDNA2 translator PRs.
- Full Xbox GDK/D3D12.X replacement: **not implemented in this project**.
- Xbox RDNA2 code translation, NVIDIA/Intel/AMD **physical GPU** semantics and actual console D3D12.X rendering: **not implemented or independently verified**. Desktop SM6 WARP success is separate.
- [Native port completion criteria](NATIVE_TARGET.md): a standard Windows executable, not an emulated console process or a game-title compatibility claim.
- Performance and gameplay claims: **none**.

## Evidence vocabulary
- `metadata`: SHA, description, license metadata and release version observed from GitHub APIs.
- `claim`: upstream author says functionality works; not necessarily independently verified.
- `source-reviewed`: implementation code inspected; correctness not established.
- `fixture-tested`: source-built synthetic test executed successfully.
- `game-tested`: game-specific, reproducible execution on identified hardware.
- `independently-reproduced`: repeat confirmed in an independent environment.

Do not equate GitHub popularity, API coverage, SPIR-V validation or commit counts with title compatibility.