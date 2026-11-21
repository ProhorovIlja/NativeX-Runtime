# Original native Windows PE32+ execution probes

Two source-built x64 executables, linked without the C runtime, exercise a separately built native AnyXboxX loader. The baseline returns **42** from its entry point; the import probe returns **43** if Win32 GetCurrentProcessId succeeds and GetModuleHandleW(NULL) points to a valid mapped MZ image. Their exact exit codes are checked with a per-child 10-second watchdog.

The inputs contain only our original source and publicly documented Win32 functions. No game, GDK, Xbox shader, console library, or third-party executable is distributed in this repository. The external AnyXboxX source remains separately licensed (LGPL-2.1-or-later) and is built at a pinned commit on ephemeral CI.

A passing test proves small desktop PE32+ execution through a native loader and Win32 import binding. It does not establish console PE/ABI compatibility, D3D12.X, RDNA2, GDK samples or GTA VI boot.

A third original executable (scarlett_pe_gdk_shim.exe) validates this pinned upstream's built xgameruntime.dll: dynamically retrieves XSystemGetDeviceType, XGameRuntimeInitialize and QueryApiImpl, checks device type 7, S_OK initialization, and E_NOINTERFACE for a null GUID request while clearing the output pointer. It returns 44 only when those checks pass. This is a specific shim function smoke test, not Xbox game/GDK compatibility.
