# Direct3D 12 host interaction through native-loaded PE32+

This source-built fixture tests a further capability of the pinned AnyXboxX native loader, above plain Win32 imports and GDK-shim export calls: **desktop DXGI and D3D12 calls from the mapped entry point**.

Test source: `fixtures/native-pe-exec/d3d12_probe.c`. It is an original CRT-free Windows x64 PE that creates a DXGI factory, WARP adapter, D3D12 device, command queue and fence; it signals and waits for the queue fence, then checks device-removal reason and releases objects. Exit code 45 is success; 101–110 are hard failures. A separate PowerShell CI parent enforces a 10-second watchdog, and the fence itself has a 5-second timeout. All operations are on **desktop Direct3D 12 WARP**, never native Xbox RDNA2 commands.

This fixture is only considered independently validated when the pinned-runtime Windows workflow reports `NATIVE_PE_PASS probe=scarlett_pe_d3d12 expected_exit=45 actual_exit=45`. A source-build alone, an unsuccessful run, or a skipped test does not meet this gate.

This is NOT a proof of source-built GDK desktop sample rendering, first Present, D3D12.X GPU command packet conversion, native hardware wave64, RDNA2 binary translation or any commercial Xbox title.

## Independently verified result — 2026-11-22

[Windows Actions run 44958262954](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44958262954) built the exact pinned external native runtime SHA and all four original x64 PE probes. The fourth probe logged:

`NATIVE_PE_PASS probe=scarlett_pe_d3d12 expected_exit=45 actual_exit=45`

The same run passed baseline/native imports/GDK shim probes with codes 42/43/44. CMake/MSVC compilation, import resolution, native execution and WARP fence signaling completed without error. The first attempted build failed on MSVC C4201 warnings in Windows SDK D3D12 C headers under `/WX`; the correction scoped that warning only around the system header includes while keeping strict warnings for original source.

This result **does not** assert an Xbox title ran, a console GDKX API was implemented, real RDNA2 shader instructions executed or a frame was presented. The next presentation test must use source-built, freely distributable Windows/GDK sample inputs, not retail game files.
