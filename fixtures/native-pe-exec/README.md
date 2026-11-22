# Original native Windows PE32+ execution probes

Five original source-built x64 executables, linked without the C runtime, exercise a separately built native AnyXboxX loader. The first two verify elementary entry and Win32 imports, while later probes cover limited GDK-compatible calls, desktop D3D12 WARP, and DXGI Present. The baseline returns **42** from its entry point; the import probe returns **43** if Win32 GetCurrentProcessId succeeds and GetModuleHandleW(NULL) points to a valid mapped MZ image. Their exact exit codes are checked with a per-child 10-second watchdog.

The inputs contain only our original source and publicly documented Win32 functions. No game, GDK, Xbox shader, console library, or third-party executable is distributed in this repository. The current pinned external AnyXboxX revision is **GPL-3.0-or-later** (its earlier history was LGPL-2.1-or-later). It is built from its own public source at a fixed commit in ephemeral CI; no GPL source or binary is distributed in this MIT repository.

A passing test proves small desktop PE32+ execution through a native loader and Win32 import binding. It does not establish console PE/ABI compatibility, D3D12.X, RDNA2, GDK samples or GTA VI boot.

A third original executable (scarlett_pe_gdk_shim.exe) validates this pinned upstream's built xgameruntime.dll: dynamically retrieves XSystemGetDeviceType, XGameRuntimeInitialize and QueryApiImpl, checks device type 7, S_OK initialization, and E_NOINTERFACE for a null GUID request while clearing the output pointer. It returns 44 only when those checks pass. This is a specific shim function smoke test, not Xbox game/GDK compatibility.


A fourth synthetic executable, `scarlett_pe_d3d12.exe`, tests the **real desktop D3D12** path from inside the manually loaded PE. It imports DXGI and D3D12, obtains the software WARP adapter, creates an ID3D12Device, a direct command queue and fence, signals fence value 1 and waits for completion. It returns **45** on success; distinct 101–110 status codes identify stage failures. The upstream loader is not an emulator; the Windows WARP backend is a software Direct3D driver rather than a hardware GPU. No game shaders or Xbox D3D12.X commands are used. This demonstrates basic PC graphics API access inside a native-loaded original program, not frame rendering or console compatibility.

## Fifth desktop PE32+ through the pinned native loader: Win32 DXGI Present

The original CRT-free `scarlett_pe_dxgi_present.exe` adds a fifth test. It imports and calls real Windows `user32.dll`, DXGI and D3D12 APIs **from inside the manually mapped PE**. It registers a Win32 class, creates a native window, uses the desktop WARP D3D12 queue to create a 64×64 two-buffer flip-discard DXGI swapchain, invokes `IDXGISwapChain1::Present(1,0)`, waits for a D3D12 fence and returns **46** only if all operations finish. Each error stage returns a distinct code **111–125** and cleans up COM objects/windows/events. The native source-build CI enforces a 10-second watchdog as for the preceding probes.

This demonstrates the pinned **external native loader** can resolve and execute additional ordinary Win32 desktop imports and reach a real swapchain Present. It does not prove a particular pixel was drawn inside this fifth loaded program: the *separate* standalone [DXGI Present fixture](../../docs/NATIVE_WIN_DXGI_PRESENT_2026-11-22.md) clears a backbuffer, and [D3D12 raster/readback](../../docs/D3D12_RASTER_READBACK_2026-11-22.md) verifies image pixels. None proves console D3D12.X, Xbox shader binary translation, game assets or GTA VI gameplay.

The upstream AnyXboxX source pin currently has **GPL-3.0-or-later** licensing; its source is built separately in ephemeral Windows CI and is not imported into this MIT repository. The original four PE fixtures and their codes 42–45 remain unchanged.

The official public Microsoft [SimpleTriangleDesktop](https://github.com/microsoft/Xbox-GDK-Samples/tree/main/Samples/IntroGraphics/SimpleTriangleDesktop) requires separate Game Development Kit toolchain/includes (`grdk.h`, `XGameRuntime`, `XTaskQueue`, DirectXTK12, PIX, etc.) and is not simply an unmodified stock Windows SDK project. We do not mislabel our original desktop D3D12 test as that official GDK sample, and do not claim to have compiled it or distributed licensed SDK libraries.
