# Additional primary public references — 2026-11-22

This document expands the repeatable public-source radar from 14 to 19 tracked repositories. Five additions were checked through GitHub's public repository and branch-ref metadata, not by downloading binaries or treating upstream demos as proven Xbox title compatibility.

| Reference | Technical value | Evidence restriction |
|---|---|---|
| [microsoft/Xbox-GDK-Samples](https://github.com/microsoft/Xbox-GDK-Samples) | Legitimate PC-GDK example programs, including SimpleTriangleDesktop, for testable native startup, GDK initialization and D3D12 output | PC desktop sample builds are not the confidential Xbox GDKX console builds |
| [microsoft/DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) | HLSL → DXIL generation, Shader Model 6 and compiler regression tests | Compiling original HLSL does not translate opaque Xbox RDNA2 ISA |
| [llvm/llvm-project](https://github.com/llvm/llvm-project) | AMDGPU ISA and compiler architecture references, legal test vector generation | AMDGPU LLVM lowering is not proof of correctness on console game shader blobs |
| [microsoft/DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples) | Native Windows D3D12 command submission, barriers, heaps, presentation, WARP tests | Ordinary PC D3D12 structures do not match undocumented D3D12.X inlined records |
| [shadps4-emu/shadPS4](https://github.com/shadps4-emu/shadPS4) | Independent GPU and memory translation architecture comparison | Different platform, PS4 GCN vs Series RDNA2, and GPL-2.0: not source for direct copy into an MIT project |

Repository-level GitHub SPDX metadata was MIT for Microsoft's GDK samples and DirectX graphics samples, GPL-2.0 for shadPS4, and NOASSERTION for LLVM and DXC. Review exact file and bundled dependency licenses before any reuse; the scanner carries UNVERIFIED for NOASSERTION.

## Reference SHAs observed
- [microsoft/Xbox-GDK-Samples @ b1e77b6ac10f](https://github.com/microsoft/Xbox-GDK-Samples/commit/b1e77b6ac10f95b07296025fda171ca2ddc42dbe): `main`, `b1e77b6ac10f95b07296025fda171ca2ddc42dbe`
- [microsoft/DirectXShaderCompiler @ 915bf4f93a25](https://github.com/microsoft/DirectXShaderCompiler/commit/915bf4f93a254d920b4bcab75b6b1c622eacd816): `main`, `915bf4f93a254d920b4bcab75b6b1c622eacd816`
- [llvm/llvm-project @ 918df2fb335a](https://github.com/llvm/llvm-project/commit/918df2fb335ac75e9b8995dcec64c6b187fbe44e): `main`, `918df2fb335ac75e9b8995dcec64c6b187fbe44e`
- [microsoft/DirectX-Graphics-Samples @ 68a2b1e97f71](https://github.com/microsoft/DirectX-Graphics-Samples/commit/68a2b1e97f71642924e74c7757ca21756ad221b5): `master`, `68a2b1e97f71642924e74c7757ca21756ad221b5`
- [shadps4-emu/shadPS4 @ 264d6b71d867](https://github.com/shadps4-emu/shadPS4/commit/264d6b71d8671a429ad2c9d1a281e18558fb9693): `main`, `264d6b71d8671a429ad2c9d1a281e18558fb9693`

## Next reproducible validation

Now that a pinned AnyXboxX launcher has actually executed our three original native PE32+ fixtures, prioritize compiling and running an official PC GDK sample such as SimpleTriangleDesktop and checking a deterministic window, rendering and shutdown. Source-build the sample with the current public GDK where possible, and state if GDK version migration prevents it.

Keep separate completion gates: (1) plain Windows PE + Win32 imports (done), (2) AnyXboxX's own GDK shim (small synthetic smoke done), (3) PC GDK sample rendering (pending), (4) actual Xbox GDKX/D3D12.X sample semantics (pending; no proprietary SDK), (5) lawfully available complete title build (not present). Do not put game EXE, shaders, secret SDK components, credentials or proprietary assets in public CI.

This report documents a selected, relevant set of sources rather than claiming exhaustive coverage of all GitHub.
