// Original Windows x64 PE32+ entry point probe. No CRT, console code or game data.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef SCARLETT_PROBE_MODE
#error "Set probe mode"
#endif
int WINAPI ProbeEntry(void) {
#if SCARLETT_PROBE_MODE == 0
    return 42;
#elif SCARLETT_PROBE_MODE == 1
    HMODULE image = GetModuleHandleW(NULL);
    if (!image || !GetCurrentProcessId()) return 91;
    if (*(const unsigned short*)image != 0x5A4D) return 92;
    return 43;
#elif SCARLETT_PROBE_MODE == 2
    // Only test this pinned source-built shim's public symbols.
    HMODULE runtime = LoadLibraryW(L"xgameruntime.dll");
    if (!runtime) return 93;
    typedef int (WINAPI *DeviceFn)(void);
    typedef HRESULT (WINAPI *InitFn)(void);
    typedef HRESULT (WINAPI *QueryFn)(const GUID*, const GUID*, void**, void*);
    DeviceFn device = (DeviceFn)GetProcAddress(runtime, "XSystemGetDeviceType");
    InitFn initialize = (InitFn)GetProcAddress(runtime, "XGameRuntimeInitialize");
    QueryFn query = (QueryFn)GetProcAddress(runtime, "QueryApiImpl");
    if (!device || !initialize || !query) return 94;
    if (device() != 7 || initialize() != S_OK) return 95;
    void* result = runtime;
    if (query(NULL, NULL, &result, NULL) != E_NOINTERFACE || result) return 96;
    return 44;
#else
#error "Unknown fixture"
#endif
}
