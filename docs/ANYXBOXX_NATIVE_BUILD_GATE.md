# Independent native Windows upstream source-build acceptance

This experiment independently builds a pinned publicly available LooperSalty/AnyXboxX C++ native runtime from an exact Git SHA. Current source-build pin: **1a2e2e677c18e6ae4053eb84d4ff2395c538c108** (2026-11-21); the original 2026-11-22 proof used **c9a243270ceb309c15fe82b0b6b763ff29061d9e**. The upstream project changed its main license from **LGPL-2.1-or-later to GPL-3.0-or-later on 2026-11-21**; different third-party components keep their own licenses. No AnyXboxX source or binary is imported into this MIT repository, and distributing a combined derivative would require an explicit compatibility/license review.

The PowerShell script tools/verify_anyxboxx_native.ps1 creates a fresh temporary directory, fetches only the pinned commit, verifies its full SHA, configures runtime/CMakeLists.txt for Windows x64, and compiles the source. It requires the native launcher and the five named C++ shim DLLs to exist after build. In build-only mode it does not execute the generated EXE; with -RunSynthetic, it runs ONLY our three source-built PE32+ probes with strict exit-code checks and 10-second watchdogs. It never runs a retail title and does not upload binary artifacts.

The independent GitHub Actions job uses standard public Windows runners, read-only repository permissions, nonpersisted checkout credentials, and triggers only when this test workflow/script changes (or manually). No paid GPU runner, game EXE, closed Xbox SDK or game assets are involved.

Important: a build system may execute its own CMake/compiler custom commands during compilation. Only run trusted, pinned upstream code in this restricted ephemeral runner; the test does not guarantee that arbitrary third-party source is harmless. A passing build demonstrates compilation. The separate -RunSynthetic mode tests execution of original desktop PE32+ entry points, Win32 imports and limited calls to this pinned upstream's GDK-shim exports; it does NOT establish an Xbox console PE loader, full GDK support, RDNA2 translation, rendered games or compatibility with GTA VI.

Original public sample tests in AnyXboxX docs use SimpleTriangleDesktop and SDL3 testgdk, both *PC GDK samples*. Reproducing those samples and then Xbox-specific D3D12.X behavior is a separate acceptance gate.


## Independent build result — 2026-11-22

Verified on standard public Windows GitHub Actions runner:
- [GitHub Actions run 44956381316](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44956381316): SUCCESS.
- Exact fetched and checked out upstream SHA: c9a243270ceb309c15fe82b0b6b763ff29061d9e.
- CMake configure, MSVC build, and file presence checks passed.
- Output log: SOURCE_BUILD_PASS, launcher size 64000 bytes, five expected runtime DLL files found.

Evidence level: **independently reproduced source build**. No PE sample startup, screen output, input/audio behavior, execution of generated launcher, Xbox D3D12.X translation, or GTA VI gameplay is claimed. This test runs in an isolated ephemeral CI environment only, no published binaries.

GitHub warned that checkout@v4 used deprecated Node.js 20; the workflow was moved to checkout@v5 with Node.js 24 for the follow-up run.


## Independent source-built PE execution result

[GitHub Actions run 44957418968](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44957418968) compiled the **exact pinned external runtime SHA** and executed three ORIGINAL programs, built on the same Windows CI runner from this MIT project's C source. All checks passed:

- `scarlett_pe_baseline.exe`: returned **42**, confirming native PE entry execution.
- `scarlett_pe_imports.exe`: returned **43**, confirming GetCurrentProcessId plus GetModuleHandleW(NULL) and a valid MZ header of the mapped image.
- `scarlett_pe_gdk_shim.exe`: returned **44**, checking this upstream's own xgameruntime.dll exports (device type 7, S_OK initialization, expected E_NOINTERFACE/null output on unsupported interface request).

Every process ran with a 10-second watchdog. These are **source-built Win32 fixtures**, not Xbox Series game binaries, PC GDK rendering demos, console GDKX interfaces or a GTA VI startup. The full test scripts and C sources are in [PR #25](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/pull/25).


## Updated upstream candidate — 2026-11-21 commit

- Public repo [LooperSalty/AnyXboxX](https://github.com/LooperSalty/AnyXboxX) commit [`1a2e2e677c18`](https://github.com/LooperSalty/AnyXboxX/commit/1a2e2e677c18e6ae4053eb84d4ff2395c538c108), source pin in `tools/verify_anyxboxx_native.ps1`.
- Upstream [PR #77](https://github.com/LooperSalty/AnyXboxX/pull/77) updates experimental **RDNA2 vector-memory operand disassembly** for MUBUF, MTBUF, FLAT/GLOBAL/SCRATCH and MIMG, backed by generated LLVM-derived test data. Author's PR claims 319 passed and 8 skipped upstream tests, with one MIMG ambiguous encoding and missing VOP3P golden vectors. These are **upstream assertions**, not independent verification of game rendering or RDNA2 shader translation.
- `THIRD_PARTY.md` in the new upstream commits explicitly documents a transition from LGPL-2.1-or-later to **GPL-3.0-or-later**, along with GPL-3.0 Leonida components. A source-built external runtime in isolated CI does not itself mix licenses. **Do not vendor or merge GPL-covered upstream implementation into an MIT-only distributable without proper licensing decisions.**
- The Windows gate still requires the native x64 launcher, five shim DLL files and our own four original PE32+ probes returning 42/43/44/45. This tests regression and elementary Win32/GDK-style API execution, **not** full RDNA2 ISA coverage, Xbox D3D12.X command submission, real title loading or GTA VI.

