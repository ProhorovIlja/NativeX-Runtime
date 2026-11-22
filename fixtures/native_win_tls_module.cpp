// Original native Win64 source-built TLS callback and per-thread state DLL.
// No game code, synthetic console binary or emulated CPU.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winnt.h>
static __declspec(thread) int tls_per_thread_value=17;
static volatile LONG tls_process_attach=0, dll_process_attach=0;
static volatile LONG tls_thread_attach=0, dll_thread_attach=0;
static volatile LONG incorrect_callback_order=0;
static volatile LONG* detach_sink=nullptr;
static void NTAPI on_tls(PVOID,DWORD reason,PVOID){
    if(reason==DLL_PROCESS_ATTACH)
        InterlockedIncrement(&tls_process_attach);
    else if(reason==DLL_THREAD_ATTACH)
        InterlockedIncrement(&tls_thread_attach);
    else if(reason==DLL_PROCESS_DETACH && detach_sink)
        InterlockedIncrement(detach_sink);
}
// Deliberately place a callback pointer in the OS PE TLS initializer list.
// These are MSVC-owned, source-built entries, not an imported GDK sample.
#pragma section(".CRT$XLB",read)
extern "C" __declspec(allocate(".CRT$XLB"))
PIMAGE_TLS_CALLBACK scarlett_tls_callback_pointer=on_tls;
#ifdef _M_X64
#pragma comment(linker,"/INCLUDE:_tls_used")
#pragma comment(linker,"/INCLUDE:scarlett_tls_callback_pointer")
#endif
BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){
        if(InterlockedCompareExchange(&tls_process_attach,0,0)!=1)
            return FALSE; // PE TLS callback must run BEFORE DllMain.
        InterlockedIncrement(&dll_process_attach);
    }else if(reason==DLL_THREAD_ATTACH){
        const LONG callbacks=InterlockedCompareExchange(
            &tls_thread_attach,0,0);
        const LONG entered=InterlockedCompareExchange(
            &dll_thread_attach,0,0);
        if(callbacks<=entered)
            InterlockedIncrement(&incorrect_callback_order);
        InterlockedIncrement(&dll_thread_attach);
    }else if(reason==DLL_PROCESS_DETACH && detach_sink){
        InterlockedIncrement(detach_sink+1);
    }
    return TRUE;
}
extern "C" __declspec(dllexport) int WINAPI ScarlettTlsCounter(unsigned which){
    switch(which){
        case 0:return static_cast<int>(
            InterlockedCompareExchange(&tls_process_attach,0,0));
        case 1:return static_cast<int>(
            InterlockedCompareExchange(&dll_process_attach,0,0));
        case 2:return static_cast<int>(
            InterlockedCompareExchange(&tls_thread_attach,0,0));
        case 3:return static_cast<int>(
            InterlockedCompareExchange(&dll_thread_attach,0,0));
        case 4:return static_cast<int>(
            InterlockedCompareExchange(&incorrect_callback_order,0,0));
        default:return -1;
    }
}
extern "C" __declspec(dllexport) int WINAPI ScarlettTlsGet(){
    return tls_per_thread_value;
}
extern "C" __declspec(dllexport) void WINAPI ScarlettTlsSet(int value){
    tls_per_thread_value=value;
}
extern "C" __declspec(dllexport) void WINAPI ScarlettTlsSetDetachSink(
    volatile LONG* counters){
    detach_sink=counters;
}
