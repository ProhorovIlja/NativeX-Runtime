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
#else
#error "Unknown fixture"
#endif
}
