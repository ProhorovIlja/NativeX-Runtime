# Independent native Windows upstream source-build acceptance

This experiment builds the publicly available LooperSalty/AnyXboxX C++ native runtime from an exact Git SHA: c9a243270ceb309c15fe82b0b6b763ff29061d9e. Upstream is licensed LGPL-2.1-or-later, with separately licensed third-party components; sources remain in the separate upstream repository and are not imported into this MIT project.

The PowerShell script tools/verify_anyxboxx_native.ps1 creates a fresh temporary directory, fetches only the pinned commit, verifies its full SHA, configures runtime/CMakeLists.txt for Windows x64, and compiles the source. It requires the native launcher and the five named C++ shim DLLs to exist after build. It never runs the compiled EXE or any retail title and does not upload binary artifacts.

The independent GitHub Actions job uses standard public Windows runners, read-only repository permissions, nonpersisted checkout credentials, and triggers only when this test workflow/script changes (or manually). No paid GPU runner, game EXE, closed Xbox SDK or game assets are involved.

Important: a build system may execute its own CMake/compiler custom commands during compilation. Only run trusted, pinned upstream code in this restricted ephemeral runner; the test does not guarantee that arbitrary third-party source is harmless. A passing build demonstrates compilation of the native launcher and DLL shim sources, NOT launch correctness, full Xbox GDK support, RDNA2 translation correctness, or compatibility with GTA VI.

Original public sample tests in AnyXboxX docs use SimpleTriangleDesktop and SDL3 testgdk, both *PC GDK samples*. Reproducing those samples and then Xbox-specific D3D12.X behavior is a separate acceptance gate.
