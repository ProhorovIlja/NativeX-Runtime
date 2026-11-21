# Native-first architecture research

The [final native Windows target](NATIVE_TARGET.md) is a normal Windows-targeted x64 executable with PC graphics/audio/filesystem/settings; merely running a console PE under compatibility DLLs is an intermediate research mode, not the final deliverable.

Xbox Series x86-64 guest code potentially executes without CPU emulation on a matching Windows x64 host. This does **not** imply an unmodified console EXE boots on desktop Windows: the host still needs correct ABI/GDK, memory allocator, executable import/TLS, filesystem, task scheduling, graphics command stream, RDNA2 GPU shader translation, audio and input.

| Layer | Proposed validation | Must not imply |
|---|---|---|
| PE32+ format | bounded header/section/import parser on generated fixtures | that a proprietary EXE was inspected |
| PE loader and GDK imports | x64 relocations, TLS, named/ordinal imports, forwarders, unwind | compatibility with undisclosed APIs |
| Xbox-specific services | synthetic XGameRuntime/XTaskQueue, file, saves, user identity | ability to sign in to Xbox services |
| Unified CPU/GPU memory | host allocation, aliasing and coherence fixtures | identical real Xbox hardware behavior |
| Graphics D3D12.X | method-level ABI verification and device/queue/barrier tests | complete console renderer |
| RDNA2 shaders | instruction differential reference and GPU runs on known fixtures | full correctness from compile success |
| Audio/input | deterministic PCM/device/hotplug tests | title-level integration |
| Performance | frame timing and GPU/CPU budgets on same workloads | unexplained faster-than-console claims |

Prefer desktop Windows D3D12 for initial correctness experiments; a Vulkan path is an optional alternative, not an assumed universal improvement. Performance measurement follows correctness.

The source registry is observational. Combining projects requires both ABI interoperability and license compatibility: GPL-2.0-only/GPL-3.0/LGPL/MIT/Apache/BSD are not interchangeable. No runtime has been selected.