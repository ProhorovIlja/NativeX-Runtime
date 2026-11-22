// Original Windows x64 SEH and dynamically registered AMD64 unwind metadata.
// Executes only hand-authored x64 host instructions and native Windows APIs.
// Never loads title bytes, emulates a CPU, patches another process or installs hooks.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winnt.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
namespace {
constexpr DWORD kExpectedException=0xE04A1701u;
constexpr SIZE_T kBlockBytes=4096u;
constexpr std::size_t kUnwindOffset=64u;
// These are original, fixed tiny AMD64 instructions under Windows x64 ABI.
// sub rsp, 40; mov ecx, custom exception; xor edx,edx; xor r8d,r8d;
// xor r9d,r9d; mov rax, RaiseException address; call rax; add rsp,40; ret.
// The 40-byte allocation preserves the 32-byte shadow space and 16-byte alignment.
using NativeFn=void (*)();
constexpr std::array<std::uint8_t,34> kCodeBase{{
    0x48,0x83,0xec,0x28,
    0xb9,0x01,0x17,0x4a,0xe0,
    0x31,0xd2,
    0x45,0x31,0xc0,
    0x45,0x31,0xc9,
    0x48,0xb8,0,0,0,0,0,0,0,0,
    0xff,0xd0,
    0x48,0x83,0xc4,0x28,
    0xc3
}};
static_assert(sizeof(void*)==8,"x64 native execution only");
static_assert(kCodeBase.size()==34u,"synthetic bytecode length");
struct Region {
    void* base=nullptr;
    RUNTIME_FUNCTION function{};
    bool added=false;
    ~Region(){
        if(added)RtlDeleteFunctionTable(&function);
        if(base)VirtualFree(base,0,MEM_RELEASE);
    }
};
int error(const char* what){
    std::cerr<<"native dynamic Win64 SEH test failure: "<<what
             <<" error="<<GetLastError()<<'\n';return 1;
}
int __declspec(noinline) catch_native_exception(NativeFn fn){
    volatile LONG reached=0;
    __try {
        reached=1;
        fn();
        return 20; // the synthetic RaiseException must NOT return
    } __except(GetExceptionCode()==kExpectedException
               ?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){
        reached=2;
    }
    return reached==2?0:21;
}
int run(){
    Region reg{};
    reg.base=VirtualAlloc(nullptr,kBlockBytes,MEM_RESERVE|MEM_COMMIT,
                          PAGE_READWRITE);
    if(!reg.base)return error("allocate writable native code region");
    auto* bytes=static_cast<std::uint8_t*>(reg.base);
    std::memcpy(bytes,kCodeBase.data(),kCodeBase.size());
    const auto raise=reinterpret_cast<std::uintptr_t>(&RaiseException);
    for(unsigned i=0;i<8;i++)
        bytes[19u+i]=static_cast<std::uint8_t>(raise>>(8u*i));
    // On x64, UNWIND_INFO: version1/no handler, prolog size 4,
    // one UWOP_ALLOC_SMALL (op=2, info=4: 8*4+8=40 bytes), code offset 4.
    const std::array<std::uint8_t,8> unwind{{
        0x01,0x04,0x01,0x00,0x04,0x42,0x00,0x00
    }};
    std::memcpy(bytes+kUnwindOffset,unwind.data(),unwind.size());
    reg.function.BeginAddress=0;
    reg.function.EndAddress=static_cast<DWORD>(kCodeBase.size());
    reg.function.UnwindData=static_cast<DWORD>(kUnwindOffset);
    // Do not retain writable-and-executable pages (W^X).
    DWORD previous=0;
    if(!VirtualProtect(reg.base,kBlockBytes,PAGE_EXECUTE_READ,&previous) ||
       previous!=PAGE_READWRITE)return error("VirtualProtect writable to RX");
    FlushInstructionCache(GetCurrentProcess(),reg.base,kCodeBase.size());
    const DWORD64 base=reinterpret_cast<DWORD64>(reg.base);
    DWORD64 image_base=0;
    // No matching runtime function must exist before explicit registration.
    if(RtlLookupFunctionEntry(base+4u,&image_base,nullptr))
        return error("dynamically allocated code unexpectedly pre-registered");
    if(!RtlAddFunctionTable(&reg.function,1,base))
        return error("RtlAddFunctionTable");
    reg.added=true;
    auto* const looked_up=RtlLookupFunctionEntry(base+4u,&image_base,nullptr);
    if(looked_up!=&reg.function || image_base!=base ||
       looked_up->BeginAddress!=0u ||
       looked_up->EndAddress!=kCodeBase.size() ||
       looked_up->UnwindData!=kUnwindOffset)
        return error("RtlLookupFunctionEntry did not resolve registered pdata");
    // The callback crosses the manually registered frame when the native
    // OS walks the stack to reach the surrounding MSVC __except handler.
    NativeFn callback=nullptr;
    static_assert(sizeof(callback)==sizeof(reg.base),"Win64 function pointer size");
    std::memcpy(&callback,&reg.base,sizeof(callback));
    if(catch_native_exception(callback)!=0)
        return error("OS did not unwind synthetic frame to real SEH handler");
    if(!RtlDeleteFunctionTable(&reg.function))
        return error("RtlDeleteFunctionTable");
    reg.added=false;
    image_base=0;
    if(RtlLookupFunctionEntry(base+4u,&image_base,nullptr))
        return error("dynamic function remained registered after deletion");
    std::cout<<"{\"kind\":\"native-win64-dynamic-seh-unwind\","
             "\"genuine_win64_native_instructions\":true,"
             "\"explicit_pdata_registered\":true,"
             "\"rtl_lookup_match\":true,"
             "\"unwind_small_stack_alloc_40\":true,"
             "\"raised_exception_caught_across_generated_frame\":true,"
             "\"function_table_unregistered\":true,"
             "\"memory_wx_permissions\":true,"
             "\"gdk_exception_semantics_verified\":false,"
             "\"xbox_game_tested\":false}\n";
    return 0;
}
}
int main(){return run();}
