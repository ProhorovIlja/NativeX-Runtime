# Development state — 2026-11-22

- Public clean repository: **yes**, new independent history.
- Imported proprietary assets/executables: **none**.
- Runnable Xbox Series title or commercial game: **not demonstrated**.
- Original read-only PE32+ inspector: Linux/Windows CI passed for baseline synthetic tests. Extended import/TLS/relocation metadata is under a separate draft PR.
- Windows-native host capability probe: public Windows x64 MSVC CI passed.
- GitHub revision/discovery tools: offline fixture and Linux CI passed.
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