# Verified native PE32+ execution — 2026-11-22

This is the first point at which a pinned public native Windows compatibility runtime was **both compiled and independently executed** with our own controlled programs rather than only checked for output binaries.

- Upstream: [LooperSalty/AnyXboxX at c9a243270ceb309c15fe82b0b6b763ff29061d9e](https://github.com/LooperSalty/AnyXboxX/commit/c9a243270ceb309c15fe82b0b6b763ff29061d9e) (LGPL-2.1-or-later; not copied into this repository).
- PR: [#25](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/25).
- GitHub Actions: [44957418968](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44957418968).
- Environment: ephemeral standard public Windows x64 runner, MSVC/CMake, no paid GPU host, no console executable.
- Code under test: three ORIGINAL `fixtures/native-pe-exec/probe.c` variants. No CRT, no game data, per-process 10-second timeout.

| Probe | What it actually tests | Expected | Observed |
|---|---|---:|---:|
| `scarlett_pe_baseline.exe` | Native loader maps PE32+ and calls the C entry point | 42 | 42 |
| `scarlett_pe_imports.exe` | Win32 GetCurrentProcessId, redirected GetModuleHandleW(NULL), MZ image magic | 43 | 43 |
| `scarlett_pe_gdk_shim.exe` | Dynamically resolved upstream XSystemGetDeviceType, XGameRuntimeInitialize, QueryApiImpl null-interface failure | 44 | 44 |

The exact CI log contains `SOURCE_BUILD_PASS`, `NATIVE_PE_PASS probe=scarlett_pe_baseline expected_exit=42 actual_exit=42`, corresponding import and GDK shim success messages.

**Boundary:** Only desktop Windows x64 code and a small subset of this upstream runtime API are shown to execute. A Win32 test that calls a source-built GDK shim does not validate real Game Core ABI, console GDKX, D3D12.X packets, RDNA2 shader ISA, GPU rendering, stateful saves, input/audio, or game progression. GTA VI executable/assets remain unavailable.

Next reproducibility gate: compile a public, lawful PC-GDK example (SimpleTriangleDesktop or SDL3 testgdk) with current official Microsoft GDK and independently verify app lifecycle plus a rendered frame. That still will not establish actual Xbox Series title compatibility.
