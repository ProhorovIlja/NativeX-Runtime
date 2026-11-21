// Original Windows/Linux library export inventory. Presence != functional integration.
#include <cstring>
#include <iostream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static bool exported(const wchar_t* dll,const char* symbol) {
    HMODULE m=LoadLibraryExW(dll,nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!m)return false;
    const bool found=GetProcAddress(m,symbol)!=nullptr;
    FreeLibrary(m);return found;
}
#else
#include <dlfcn.h>
static bool exported(const char* dll,const char* symbol) {
    void* m=dlopen(dll,RTLD_NOW|RTLD_LOCAL);
    if(!m)return false;
    const bool found=dlsym(m,symbol)!=nullptr;
    dlclose(m);return found;
}
#endif
int main(int argc,char** argv) {
    const bool x64=sizeof(void*)==8;
    if(argc>1&&std::strcmp(argv[1],"--self-test")==0) {
        if(!x64)return 2;
#ifdef _WIN32
        if(!exported(L"kernel32.dll","GetNativeSystemInfo"))return 3;
#endif
        std::cout<<"native host inventory self-test PASS\n";
        return 0;
    }
#ifdef _WIN32
    const char* os="windows";
    const bool d3d=exported(L"d3d12.dll","D3D12CreateDevice");
    const bool dxgi=exported(L"dxgi.dll","CreateDXGIFactory2");
    const bool vk=exported(L"vulkan-1.dll","vkGetInstanceProcAddr");
    const bool ds=exported(L"dstorage.dll","DStorageGetFactory");
    const bool audio=exported(L"xaudio2_9.dll","XAudio2Create");
    const bool gameinput=exported(L"GameInput.dll","GameInputCreate");
    const bool aio=false;
#else
    const char* os="linux";
    const bool d3d=false,dxgi=false,ds=false,gameinput=false;
    const bool vk=exported("libvulkan.so.1","vkGetInstanceProcAddr");
    const bool audio=exported("libpipewire-0.3.so.0","pw_init") ||
                     exported("libasound.so.2","snd_pcm_open");
    const bool aio=exported("liburing.so.2","io_uring_queue_init") ||
                   exported("liburing.so.1","io_uring_queue_init");
#endif
    std::cout<<"{\"kind\":\"native-host-module-presence-only\","
             <<"\"platform\":\""<<os<<"\",\"x64\":"<<(x64?"true":"false")<<","
             <<"\"exports\":{"
             <<"\"d3d12\":"<<(d3d?"true":"false")<<","
             <<"\"dxgi\":"<<(dxgi?"true":"false")<<","
             <<"\"vulkan\":"<<(vk?"true":"false")<<","
             <<"\"directstorage\":"<<(ds?"true":"false")<<","
             <<"\"audio_host\":"<<(audio?"true":"false")<<","
             <<"\"gameinput\":"<<(gameinput?"true":"false")<<","
             <<"\"io_uring\":"<<(aio?"true":"false")
             <<"},\"provider_sdk_integrated\":false,\"console_game_tested\":false}\n";
    return x64?0:2;
}
