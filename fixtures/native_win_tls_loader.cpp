// Original real Windows LoadLibrary DLL TLS/process/thread lifecycle oracle.
// Verifies that PE TLS callbacks precede DllMain and independent thread slots.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <array>
#include <cstring>
#include <iostream>
#include <type_traits>
namespace {
using Counter=int(WINAPI*)(unsigned);
using Get=int(WINAPI*)();
using Set=void(WINAPI*)(int);
using SetSink=void(WINAPI*)(volatile LONG*);
template<typename T>
T symbol(HMODULE library,const char* name){
    const FARPROC address=GetProcAddress(library,name);
    T result=nullptr;
    static_assert(sizeof(address)==sizeof(result),"Win64 function pointers");
    std::memcpy(&result,&address,sizeof(result));
    return result;
}
struct Worker{
    Get get=nullptr;
    Set set=nullptr;
};
DWORD WINAPI worker_entry(void* raw){
    auto* w=static_cast<Worker*>(raw);
    if(w->get()!=17)return 21u;
    w->set(77);
    return w->get()==77?0u:22u;
}
int failure(const char* step){
    std::cerr<<"Native Win64 TLS lifecycle FAILED: "<<step
             <<" os_error="<<GetLastError()<<'\n';return 1;
}
int run(const char* path){
    HMODULE module=LoadLibraryExA(path,nullptr,
             LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!module)return failure("LoadLibraryEx source-built DLL");
    const Counter count=symbol<Counter>(module,"ScarlettTlsCounter");
    const Get get=symbol<Get>(module,"ScarlettTlsGet");
    const Set set=symbol<Set>(module,"ScarlettTlsSet");
    const SetSink sink=symbol<SetSink>(module,"ScarlettTlsSetDetachSink");
    if(!count || !get || !set || !sink){
        FreeLibrary(module);
        return failure("GetProcAddress source exports");
    }
    if(count(0)!=1 || count(1)!=1 || count(4)!=0 || get()!=17){
        FreeLibrary(module);
        return failure("initial TLS callback must precede DLL_PROCESS_ATTACH");
    }
    set(31);
    if(get()!=31){
        FreeLibrary(module);
        return failure("main-thread TLS write/read");
    }
    Worker worker{get,set};
    HANDLE thread=CreateThread(nullptr,0,worker_entry,&worker,0,nullptr);
    if(!thread){
        FreeLibrary(module);
        return failure("CreateThread during native DLL lifetime");
    }
    const DWORD waited=WaitForSingleObject(thread,10000);
    DWORD exit_code=~0u;
    const BOOL retrieved=GetExitCodeThread(thread,&exit_code);
    CloseHandle(thread);
    if(waited!=WAIT_OBJECT_0 || !retrieved || exit_code!=0u){
        FreeLibrary(module);
        return failure("worker thread independent TLS initialization and updates");
    }
    if(get()!=31 || count(2)<1 || count(3)<1 || count(4)!=0){
        FreeLibrary(module);
        return failure("thread-local isolation or TLS-before-DllMain order");
    }
    std::array<LONG,2> events{{0,0}};
    sink(events.data());
    if(!FreeLibrary(module))
        return failure("FreeLibrary original fixture");
    if(events[0]!=1 || events[1]!=1)
        return failure("TLS and DllMain process detach both executed");
    std::cout<<"{\"kind\":\"native-win64-pe-tls-dll-lifecycle\","
             "\"real_windows_loadlibrary\":true,"
             "\"tls_process_attach_before_dllmain\":true,"
             "\"thread_attach_order_verified\":true,"
             "\"independent___declspec_thread_state\":true,"
             "\"tls_and_dllmain_process_detach\":true,"
             "\"one_worker_thread_completed\":true,"
             "\"third_party_loader_tested\":false,"
             "\"xbox_gamecore_tested\":false}\n";
    return 0;
}
}
int main(int argc,char** argv){
    if(argc!=2){std::cerr<<"usage: native_win_tls_loader <compiled DLL>\n";return 2;}
    return run(argv[1]);
}
