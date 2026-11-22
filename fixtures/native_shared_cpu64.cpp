// Original native Windows/Linux 64 KiB CPU shared mapping and child-process proof.
// This tests host CPU virtual-memory aliasing, not Xbox UMA GPU coherency.
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cwchar>
#else
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#endif

namespace {
constexpr std::size_t PageBytes=65536;
std::uint8_t initial(std::size_t i) noexcept {
    return static_cast<std::uint8_t>((i*13u+17u)%251u);
}
std::uint8_t changed(std::size_t i) noexcept {
    return static_cast<std::uint8_t>((initial(i)^0xA5u)+3u);
}
bool verify(const void* data,bool final_pattern) noexcept {
    if(!data)return false;
    const auto* p=static_cast<const std::uint8_t*>(data);
    for(std::size_t i=0;i<PageBytes;i++)
        if(p[i]!=(final_pattern?changed(i):initial(i)))return false;
    return true;
}
void fill(void* ptr,bool final_pattern) noexcept {
    auto* p=static_cast<std::uint8_t*>(ptr);
    for(std::size_t i=0;i<PageBytes;i++)
        p[i]=final_pattern?changed(i):initial(i);
}
#ifdef _WIN32
struct Mapping {
    HANDLE handle=nullptr;
    void* first=nullptr;
    void* second=nullptr;
    ~Mapping(){
        if(second)UnmapViewOfFile(second);
        if(first)UnmapViewOfFile(first);
        if(handle)CloseHandle(handle);
    }
};
int child(const wchar_t* name){
    Mapping m{};
    m.handle=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
    if(!m.handle)return 101;
    m.first=MapViewOfFile(m.handle,FILE_MAP_ALL_ACCESS,0,0,PageBytes);
    if(!verify(m.first,false))return 102;
    fill(m.first,true);
    return verify(m.first,true)?0:103;
}
int parent(){
    const std::wstring name=L"Local\\ScarlettNativeShare64_"+
                            std::to_wstring(GetCurrentProcessId())+L"_"+
                            std::to_wstring(GetTickCount64());
    Mapping m{};
    m.handle=CreateFileMappingW(INVALID_HANDLE_VALUE,nullptr,PAGE_READWRITE,
                                0,static_cast<DWORD>(PageBytes),name.c_str());
    if(!m.handle || GetLastError()==ERROR_ALREADY_EXISTS)
        throw std::runtime_error("CreateFileMappingW shared section failed");
    m.first=MapViewOfFile(m.handle,FILE_MAP_ALL_ACCESS,0,0,PageBytes);
    m.second=MapViewOfFile(m.handle,FILE_MAP_ALL_ACCESS,0,0,PageBytes);
    if(!m.first||!m.second || m.first==m.second)
        throw std::runtime_error("two distinct CPU mappings unavailable");
    fill(m.first,false);
    if(!verify(m.second,false))
        throw std::runtime_error("second CPU view did not see initial bytes");
    std::array<wchar_t,32768> self{};
    const DWORD count=GetModuleFileNameW(nullptr,self.data(),
                                         static_cast<DWORD>(self.size()));
    if(!count || count>=self.size()-1)
        throw std::runtime_error("self executable path unavailable");
    std::wstring command=L"\""+std::wstring(self.data(),count)+L"\" --child "+name;
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    PROCESS_INFORMATION proc{};
    if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,
                       CREATE_NO_WINDOW,nullptr,nullptr,&startup,&proc))
        throw std::runtime_error("CreateProcessW child failed");
    CloseHandle(proc.hThread);
    const DWORD wait=WaitForSingleObject(proc.hProcess,10000);
    if(wait!=WAIT_OBJECT_0){
        TerminateProcess(proc.hProcess,199);
        CloseHandle(proc.hProcess);
        throw std::runtime_error("child process timed out");
    }
    DWORD code=99;
    const BOOL got=GetExitCodeProcess(proc.hProcess,&code);
    CloseHandle(proc.hProcess);
    if(!got || code!=0)
        throw std::runtime_error("child mapping check failed code="+std::to_string(code));
    if(!verify(m.first,true)||!verify(m.second,true))
        throw std::runtime_error("parent views missed child mutation");
    UnmapViewOfFile(m.first);m.first=nullptr;
    m.first=MapViewOfFile(m.handle,FILE_MAP_READ,0,0,PageBytes);
    if(!verify(m.first,true))
        throw std::runtime_error("new CPU view missed section data");
    std::cout<<"{\"kind\":\"native-win-shared-cpu64\","
             "\"mapped_bytes\":"<<PageBytes<<","
             "\"independent_virtual_views\":2,"
             "\"separate_process_verified\":true,"
             "\"reopened_view_verified\":true,"
             "\"gpu_uma_coherency_verified\":false,"
             "\"game_tested\":false}\n";
    return 0;
}
#else
struct Mapping {
    int fd=-1;
    void* first=MAP_FAILED;
    void* second=MAP_FAILED;
    std::string name{};
    ~Mapping(){
        if(first!=MAP_FAILED)munmap(first,PageBytes);
        if(second!=MAP_FAILED)munmap(second,PageBytes);
        if(fd>=0)close(fd);
        if(!name.empty())shm_unlink(name.c_str());
    }
};
int child(const std::string& name){
    const int fd=shm_open(name.c_str(),O_RDWR,0);
    if(fd<0)return 101;
    void* read= mmap(nullptr,PageBytes,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);
    close(fd);
    if(read==MAP_FAILED)return 102;
    const bool good=verify(read,false);
    if(good)fill(read,true);
    const bool checked=good&&verify(read,true);
    munmap(read,PageBytes);
    return checked?0:103;
}
int parent(){
    Mapping m{};
    m.name="/scarlett-native-share64-"+std::to_string(getpid())+"-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    m.fd=shm_open(m.name.c_str(),O_CREAT|O_EXCL|O_RDWR,0600);
    if(m.fd<0)throw std::runtime_error("shm_open failed");
    if(ftruncate(m.fd,static_cast<off_t>(PageBytes))!=0)
        throw std::runtime_error("ftruncate failed");
    m.first=mmap(nullptr,PageBytes,PROT_READ|PROT_WRITE,MAP_SHARED,m.fd,0);
    m.second=mmap(nullptr,PageBytes,PROT_READ|PROT_WRITE,MAP_SHARED,m.fd,0);
    if(m.first==MAP_FAILED||m.second==MAP_FAILED||m.first==m.second)
        throw std::runtime_error("two MAP_SHARED mappings unavailable");
    fill(m.first,false);
    if(!verify(m.second,false))
        throw std::runtime_error("second CPU view did not see initial bytes");
    const pid_t pid=fork();
    if(pid<0)throw std::runtime_error("fork failed");
    if(pid==0){
        // Child opens a fresh fd and a fresh mapping by name rather than
        // reading the inherited parent mapping; no shared heap runtime.
        _exit(child(m.name));
    }
    int status=0;
    bool done=false;
    for(int i=0;i<1000;i++){
        const pid_t w=waitpid(pid,&status,WNOHANG);
        if(w==pid){done=true;break;}
        if(w<0)break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if(!done){
        kill(pid,SIGKILL);
        waitpid(pid,&status,0);
        throw std::runtime_error("child process timed out");
    }
    if(!WIFEXITED(status)||WEXITSTATUS(status)!=0)
        throw std::runtime_error("child shared mapping check failed");
    if(!verify(m.first,true)||!verify(m.second,true))
        throw std::runtime_error("parent views missed child mutation");
    munmap(m.first,PageBytes);m.first=MAP_FAILED;
    m.first=mmap(nullptr,PageBytes,PROT_READ,MAP_SHARED,m.fd,0);
    if(!verify(m.first,true))
        throw std::runtime_error("fresh read-only CPU view missed child data");
    std::cout<<"{\"kind\":\"native-linux-elf-shared-cpu64\","
             "\"mapped_bytes\":"<<PageBytes<<","
             "\"independent_virtual_views\":2,"
             "\"separate_process_verified\":true,"
             "\"reopened_view_verified\":true,"
             "\"gpu_uma_coherency_verified\":false,"
             "\"game_tested\":false}\n";
    return 0;
}
#endif
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    try{
        if(argc==3 && std::wcscmp(argv[1],L"--child")==0)return child(argv[2]);
        if(argc!=1)return 2;
        return parent();
    }catch(const std::exception& error){
        std::cerr<<"native CPU shared section FAILED: "<<error.what()<<"\n";
        return 1;
    }
}
#else
int main(int argc,char** /*argv*/){
    try{
        if(argc!=1)return 2;
        return parent();
    }catch(const std::exception& error){
        std::cerr<<"native CPU shared section FAILED: "<<error.what()<<"\n";
        return 1;
    }
}
#endif
