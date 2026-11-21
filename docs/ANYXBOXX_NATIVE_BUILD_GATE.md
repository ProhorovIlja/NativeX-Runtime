# Independent native Windows upstream source-build acceptance

This experiment builds the publicly available LooperSalty/AnyXboxX C++ native runtime from an exact Git SHA: c9a243270ceb309c15fe82b0b6b763ff29061d9e. Upstream is licensed LGPL-2.1-or-later, with separately licensed third-party components; sources remain in the separate upstream repository and are not imported into this MIT project.

The PowerShell script tools/verify_anyxboxx_native.ps1 creates a fresh temporary directory, fetches only the pinned commit, verifies its full SHA, configures runtime/CMakeLists.txt for Windows x64, and compiles the source. It requires the native launcher and the five named C++ shim DLLs to exist after build. It never runs the compiled EXE or any retail title and does not upload binary artifacts.

The independent GitHub Actions job uses standard public Windows runners, read-only repository permissions, nonpersisted checkout credentials, and triggers only when this test workflow/script changes (or manually). No paid GPU runner, game EXE, closed Xbox SDK or game assets are involved.

Important: a build system may execute its own CMake/compiler custom commands during compilation. Only run trusted, pinned upstream code in this restricted ephemeral runner; the test does not guarantee that arbitrary third-party source is harmless. A passing build demonstrates compilation of the native launcher and DLL shim sources, NOT launch correctness, full Xbox GDK support, RDNA2 translation correctness, or compatibility with GTA VI.

Original public sample tests in AnyXboxX docs use SimpleTriangleDesktop and SDL3 testgdk, both *PC GDK samples*. Reproducing those samples and then Xbox-specific D3D12.X behavior is a separate acceptance gate.


## Independent build result — 2026-11-22

Verified on standard public Windows GitHub Actions runner:
- [GitHub Actions run 44956381316](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44956381316): SUCCESS.
- Exact fetched and checked out upstream SHA: c9a243270ceb309c15fe82b0b6b763ff29061d9e.
- CMake configure, MSVC build, and file presence checks passed.
- Output log: SOURCE_BUILD_PASS, launcher size 64000 bytes, five expected runtime DLL files found.

Evidence level: **independently reproduced source build**. No PE sample startup, screen output, input/audio behavior, execution of generated launcher, Xbox D3D12.X translation, or GTA VI gameplay is claimed. This test runs in an isolated ephemeral CI environment only, no published binaries.

GitHub warned that checkout@v4 used deprecated Node.js 20; the workflow was moved to checkout@v5 with Node.js 24 for the follow-up run.
