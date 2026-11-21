# Definition of the eventual native Windows deliverable

The final deliverable must be a **normal Windows x64 application** with native Win32 graphics, audio, input, filesystem, save and lifecycle behavior. No Xbox firmware, VM, Unicorn interpreter, WSL, console process manager or CPU emulator should be required at runtime. If extra project DLLs are necessary, they must be ordinary Windows-native modules with documented redistributable licenses.

This distinction is material:

| Stage | Meaning | Status |
|---|---|---|
| Host research | Original Windows C++17 tools, PE inventory and synthetic D3D12/WARP tests | **Demonstrated on free public CI** |
| Compatibility execution | Direct execution of x64 console instructions assisted by native shim DLLs implementing Xbox APIs | **External upstream experiments, not reproduced here** |
| Actual native Windows port | Windows-targeted executable/runtime built or recompiled for PC with Xbox-specific dependencies removed/adapted, Windows renderer and standard PC saves/settings | **Not implemented** |
| Playable game | Repeatable boot, interactive rendering, input, progression and saves for a lawful matching build | **No title files supplied; unverified** |

Even though Xbox Series X uses x86-64, console EXEs depend on a different OS, ABI, GDK runtime and graphics interface; simply changing the file extension to .exe does nothing. Native x64 compatibility work is a useful stepping stone but is **not equivalent** to delivering a finished PC port.

## GPU path

Xbox RDNA2 machine-code shaders do not run unchanged on arbitrary Windows GPUs, especially NVIDIA and Intel. One candidate architecture is an offline source-backed compiler that converts the necessary supported instruction semantics to HLSL/DXIL, caches validated results and uses desktop D3D12. A native Vulkan implementation is possible but must not be assumed faster or more compatible without tests.

Our tests so far prove only:

- CPU wave64 vs split wave32 *reference algorithm*;
- desktop Windows D3D12 SM5 compute and CPU readback on WARP;
- **real desktop Shader Model 6 WaveOps execution on WARP** for source-built HLSL, with CPU comparison.

They do **not** prove console RDNA2 binary translation, D3D12.X draw/Present behavior, true 64-lane RDNA2 on hardware, a Rockstar game startup or a game FPS claim. See [SM6 fixture](NATIVE_SM6_WAVE.md).

## Evidence gates before a title-specific runtime

1. Source-only and public sample verification: machine type, loader/imports, exact source-built GDK sample tests, no retail binaries.
2. Lawfully available *matching* game executable **and** asset set: local read-only inventory, validated version/provenance/hashes, no uploads into public GitHub or Actions.
3. Host ABI/import validation for that exact build: SIDs, imports, PE relocation and TLS/unwind, platform services, filesystem and saves.
4. D3D12.X API and RDNA2 shader correctness: test vectors, per-vendor behavior, first submitted draw/present, audio and input.
5. Native PC artifact: no console process layer; recoverable saves, Windows user settings, full restart and deterministic tests.
6. Only after gameplay stability, measure CPU/GPU timings and investigate further features, including optional co-op.

## Public GDK boundary

The [official public Microsoft GDK](https://github.com/microsoft/GDK) provides Win32 + GDK development for Windows. It expressly distinguishes licensed confidential Xbox Extensions (GDKX) needed for console targeting, so the public GDK alone does **not** supply all console-only runtime/graphics interfaces. As of October 2026, Microsoft is migrating its public GDK packages to a simplified directory layout. Do not treat old GDK paths or private Xbox SDK symbols as public dependencies.

## Licensing and distribution

This repository contains only original, source-built tests and research metadata. GPL-2.0-only, GPL-3.0, LGPL and MIT/Apache/BSD source trees cannot be merged without project-specific license compliance. Private ownership or a public repo does not make a leaked retail executable freely redistributable. The final PC port is a research target, not a current compatibility claim.
