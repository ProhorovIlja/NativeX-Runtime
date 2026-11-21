# Development state — 2026-11-22

- Public clean repository: **yes**, new independent history.
- Imported proprietary assets/executables: **none**.
- Runnable Xbox Series title or commercial game: **not demonstrated**.
- Original read-only PE32+ inspector: Linux/Windows CI passed for baseline synthetic tests. Extended import/TLS/relocation metadata and corruption rejection passed on Linux and Windows in public CI.
- Windows-native host capability probe: public Windows x64 MSVC CI passed.
- GitHub revision/discovery/commit-subject scanners: offline tests and public Linux CI passed; daily bounded API reports configured.
- Wave64 vs split-wave32 CPU differential harness: source-built 330 deterministic cases, Linux/Windows CI passed, **GPU wave correctness not claimed**.
- Desktop D3D12 WARP: native Windows device, queue/fence, Shader Model 5 compute dispatch and CPU readback passed on free Windows runner ([run 44951977570](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44951977570)); **not Xbox D3D12.X or RDNA2**.
- Source audit: [dated upstream review](UPSTREAM_REVIEW_2026-11-22.md); seven tracked branch heads changed since original baseline; no external runtime selected.
- Native Windows named/ordinal DLL loader fixture: source-built test passed in Windows CI.
- Full Xbox GDK/D3D12.X replacement: **not implemented in this project**.
- Shader translation / GPU correctness: **not implemented in this project**.
- Performance and gameplay claims: **none**.

## Evidence vocabulary
- `metadata`: SHA, description, license metadata and release version observed from GitHub APIs.
- `claim`: upstream author says functionality works; not necessarily independently verified.
- `source-reviewed`: implementation code inspected; correctness not established.
- `fixture-tested`: source-built synthetic test executed successfully.
- `game-tested`: game-specific, reproducible execution on identified hardware.
- `independently-reproduced`: repeat confirmed in an independent environment.

Do not equate GitHub popularity, API coverage, SPIR-V validation or commit counts with title compatibility.