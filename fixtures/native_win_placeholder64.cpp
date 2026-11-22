// Original Windows x64 contiguous placeholder reserve/split/map/unmap regression.
// Actual VirtualAlloc2 + MapViewOfFile3 + UnmapViewOfFile2 on host CPU memory.
// No Xbox guest memory, actual GPU UMA, GDK APIs or proprietary data.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
namespace {
constexpr std::size_t Slots=4;
constexpr std::size_t BackingPages=2;
constexpr SIZE_T PageBytes=65536u;
using Reserve=void* (WINAPI*)(HANDLE,void*,SIZE_T,ULONG,ULONG,void*,ULONG);
using Map3=void* (WINAPI*)(HANDLE,HANDLE,void*,ULONG64,SIZE_T,ULONG,ULONG,void*,ULONG);
using Unmap2=BOOL (WINAPI*)(HANDLE,void*,ULONG);
struct Api {Reserve reserve=nullptr;Map3 map=nullptr;Unmap2 unmap=nullptr;};
void* load(HMODULE module,const char* symbol){
    return module?reinterpret_cast<void*>(GetProcAddress(module,symbol)):nullptr;
}
template<typename Fn>
Fn get(HMODULE module,const char* symbol){
    const FARPROC pointer=module?GetProcAddress(module,symbol):nullptr;
    Fn fn=nullptr;
    if(pointer){
        static_assert(sizeof(fn)==sizeof(pointer),"Win64 function pointer size");
        std::memcpy(&fn,&pointer,sizeof(fn));
    }
    return fn;
}
Api open_api(){
    const HMODULE base=GetModuleHandleW(L"kernelbase.dll");
    const HMODULE kernel=GetModuleHandleW(L"kernel32.dll");
    Api result{};
    result.reserve=get<Reserve>(base,"VirtualAlloc2");
    result.map=get<Map3>(base,"MapViewOfFile3");
    result.unmap=get<Unmap2>(base,"UnmapViewOfFile2");
    if(!result.reserve)result.reserve=get<Reserve>(kernel,"VirtualAlloc2");
    if(!result.map)result.map=get<Map3>(kernel,"MapViewOfFile3");
    if(!result.unmap)result.unmap=get<Unmap2>(kernel,"UnmapViewOfFile2");
    return result;
}
int fail(const char* what){
    std::cerr<<"Win32 placeholder mapping "<<what<<" error="<<GetLastError()<<'\n';
    return 1;
}
bool is_reserved(void* address){
    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(address,&info,sizeof(info))==sizeof(info) &&
           info.State==MEM_RESERVE && info.Protect==0;
}
std::uint8_t pattern(std::size_t i,std::uint8_t salt){
    return static_cast<std::uint8_t>((i*37u+salt)%251u);
}
bool verify(const void* address,std::uint8_t salt){
    const auto* p=static_cast<const std::uint8_t*>(address);
    for(std::size_t i=0;i<PageBytes;i++)
        if(p[i]!=pattern(i,salt))return false;
    return true;
}
void fill(void* address,std::uint8_t salt){
    auto* p=static_cast<std::uint8_t*>(address);
    for(std::size_t i=0;i<PageBytes;i++)p[i]=pattern(i,salt);
}
struct PageRegion {
    Api api{};
    std::uint8_t* base=nullptr;
    HANDLE section=nullptr;
    std::size_t parts=0;
    std::array<bool,Slots> mapped{};
    ~PageRegion(){
        if(base){
            for(std::size_t i=0;i<parts;i++){
                auto* current=base+i*PageBytes;
                if(mapped[i])api.unmap(GetCurrentProcess(),current,
                                        MEM_PRESERVE_PLACEHOLDER);
                VirtualFree(current,0,MEM_RELEASE);
            }
        }
        if(section)CloseHandle(section);
    }
    bool map_slot(std::size_t slot,std::size_t backing){
        if(!base || !section || slot>=Slots || backing>=BackingPages || mapped[slot])
            return false;
        void* addr=base+slot*PageBytes;
        void* actual=api.map(section,GetCurrentProcess(),addr,
                            static_cast<ULONG64>(backing*PageBytes),
                            PageBytes,MEM_REPLACE_PLACEHOLDER,
                            PAGE_READWRITE,nullptr,0);
        if(actual!=addr)return false;
        mapped[slot]=true;
        return true;
    }
    bool unmap_slot(std::size_t slot){
        if(!base || slot>=Slots || !mapped[slot])return false;
        if(!api.unmap(GetCurrentProcess(),base+slot*PageBytes,
                      MEM_PRESERVE_PLACEHOLDER))return false;
        mapped[slot]=false;return true;
    }
};
int run(){
    SYSTEM_INFO system{};
    GetNativeSystemInfo(&system);
    if(sizeof(void*)!=8 ||
       system.wProcessorArchitecture!=PROCESSOR_ARCHITECTURE_AMD64)
        return 77;
    if(system.dwAllocationGranularity!=PageBytes){
        std::cout<<"SKIP: nonstandard host allocation granularity\n";return 77;
    }
    const Api api=open_api();
    if(!api.reserve || !api.map || !api.unmap){
        std::cout<<"SKIP: Windows placeholder API unavailable on host OS\n";return 77;
    }
    PageRegion region{};region.api=api;
    region.base=static_cast<std::uint8_t*>(
        api.reserve(nullptr,nullptr,Slots*PageBytes,
                    MEM_RESERVE|MEM_RESERVE_PLACEHOLDER,
                    PAGE_NOACCESS,nullptr,0));
    if(!region.base){
        const DWORD code=GetLastError();
        if(code==ERROR_NOT_SUPPORTED||code==ERROR_CALL_NOT_IMPLEMENTED){
            std::cout<<"SKIP: placeholder reservation unsupported\n";return 77;
        }
        return fail("VirtualAlloc2 reservation");
    }
    region.parts=1; // first reservation owns all four contiguous slots
    if(!is_reserved(region.base))return fail("initial placeholder state");
    // Split one large placeholder into four individually replaceable regions.
    for(std::size_t i=0;i+1<Slots;i++){
        auto* next=region.base+i*PageBytes;
        if(!VirtualFree(next,PageBytes,MEM_RELEASE|MEM_PRESERVE_PLACEHOLDER))
            return fail("VirtualFree split reservation");
        ++region.parts;
    }
    for(std::size_t i=0;i<Slots;i++){
        if(!is_reserved(region.base+i*PageBytes))
            return fail("split placeholder still reserved");
    }
    region.section=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,
                                      0,BackingPages*PageBytes,nullptr);
    if(!region.section)return fail("CreateFileMappingW backing section");
    // Mapping order is deliberately not contiguous or monotonic.
    if(!region.map_slot(3,0)||!region.map_slot(1,1))
        return fail("MapViewOfFile3 out-of-order backing pages");
    fill(region.base+3*PageBytes,13);
    fill(region.base+PageBytes,79);
    if(!verify(region.base+3*PageBytes,13)||
       !verify(region.base+PageBytes,79))
        return fail("original 64 KiB source pages");

    // Replace slot0 with the SAME section offset as slot3: real VM alias.
    if(!region.map_slot(0,0))
        return fail("MapViewOfFile3 alias to same CPU backing tile");
    if(!verify(region.base,13))
        return fail("CPU alias did not share backing page");
    fill(region.base,27);
    if(!verify(region.base+3*PageBytes,27))
        return fail("writing alias was not visible through other virtual address");

    // Bad mappings must fail without replacing an already mapped view
    // or modifying any nearby virtual-address reservation.
    void* invalid=api.map(region.section,GetCurrentProcess(),
                          region.base,0,PageBytes,
                          MEM_REPLACE_PLACEHOLDER,PAGE_READWRITE,nullptr,0);
    if(invalid || !verify(region.base,27))
        return fail("overlapping mapped view incorrectly replaced or mutated");
    invalid=api.map(region.section,GetCurrentProcess(),
                    region.base+2*PageBytes,0,PageBytes/2,
                    MEM_REPLACE_PLACEHOLDER,PAGE_READWRITE,nullptr,0);
    if(invalid||!is_reserved(region.base+2*PageBytes))
        return fail("non-exact placeholder replacement was incorrectly accepted");

    // Return slot3 to a placeholder, preserving both address and backing bytes.
    if(!region.unmap_slot(3) || !is_reserved(region.base+3*PageBytes))
        return fail("UnmapViewOfFile2 did not preserve placeholder");
    if(!verify(region.base,27))
        return fail("unmapping an alias changed the surviving backing section");
    if(!region.map_slot(2,1)||!verify(region.base+2*PageBytes,79))
        return fail("reusing second backing page at a new reserved slot");
    if(!region.unmap_slot(1)||!is_reserved(region.base+PageBytes))
        return fail("unmap old second backing view");
    fill(region.base+2*PageBytes,111);
    if(!verify(region.base+2*PageBytes,111))
        return fail("post-move backing content lost");

    // Restore another view of the *same* first backing page to its original
    // slot after a round trip. Validate all 64 KiB bytes again.
    if(!region.map_slot(3,0) || !verify(region.base+3*PageBytes,27))
        return fail("original slot remapped to correct existing backing page");
    if(!is_reserved(region.base+PageBytes))
        return fail("unmapped slot unexpectedly committed");
    std::cout<<"{\"kind\":\"native-win-placeholder64-mapping\","
             "\"contiguous_virtual_slots\":"<<Slots<<","
             "\"page_bytes\":"<<PageBytes<<","
             "\"backing_pages\":"<<BackingPages<<","
             "\"actual_virtualalloc2\":true,"
             "\"actual_mapviewoffile3\":true,"
             "\"unmap_preserved_placeholder\":true,"
             "\"out_of_order_mapping\":true,"
             "\"same_backing_multiple_views\":true,"
             "\"invalid_replacement_rejected\":true,"
             "\"backing_bytes_preserved_across_remap\":true,"
             "\"gpu_uma_coherency_verified\":false,"
             "\"gta_vi_executed\":false}\n";
    return 0;
}
}
int main(){return run();}
