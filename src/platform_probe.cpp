// Windows-only host API availability test. Does not execute any game file.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <iostream>
struct Facility { const wchar_t* dll; const char* symbol; const char* name; };
static bool present(Facility f) {
    HMODULE m=LoadLibraryExW(f.dll,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!m)return false;
    bool ok=GetProcAddress(m,f.symbol)!=nullptr;
    FreeLibrary(m);return ok;
}
int main(int argc,char** argv){
    SYSTEM_INFO si{};GetNativeSystemInfo(&si);
    const bool x64=sizeof(void*)==8 && si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64;
    if(argc>1 && std::strcmp(argv[1],"--self-test")==0){
        const Facility f{L"kernel32.dll","GetNativeSystemInfo","kernel32"};
        if(!x64||!present(f))return 1;
        std::cout<<"Windows x64 host test PASS\n";return 0;
    }
    const Facility facilities[]={
        {L"d3d12.dll","D3D12CreateDevice","d3d12"},
        {L"dxgi.dll","CreateDXGIFactory2","dxgi"},
        {L"dxcompiler.dll","DxcCreateInstance","dxc"},
        {L"dstorage.dll","DStorageGetFactory","directstorage"},
        {L"GameInput.dll","GameInputCreate","gameinput"},
        {L"xinput1_4.dll","XInputGetState","xinput"},
        {L"xgameruntime.dll","XGameRuntimeInitialize","xgameruntime"},
        {L"vulkan-1.dll","vkGetInstanceProcAddr","vulkan"}
    };
    std::cout<<"{\"kind\":\"windows-prerequisites\",\"x64_host\":"<<(x64?"true":"false")<<",\"modules\":{";
    for(size_t i=0;i<sizeof(facilities)/sizeof(facilities[0]);i++)
        std::cout<<(i?",":"")<<"\""<<facilities[i].name<<"\":"
                 <<(present(facilities[i])?"true":"false");
    std::cout<<"},\"game_tested\":false}\n";
    return x64?0:2;
}
