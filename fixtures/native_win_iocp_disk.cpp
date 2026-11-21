// Original Win32 x64 IO completion port test with real overlapped disk I/O.
// No DirectStorage/GDeflate or game asset use. Temporary file deleted on close.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
constexpr DWORD kChunks=8;
constexpr DWORD kChunkBytes=4096;
constexpr DWORD kTotalBytes=kChunks*kChunkBytes;
constexpr ULONG_PTR kCompletionKey=0x53534343u;
struct AutoHandle {
    HANDLE handle=INVALID_HANDLE_VALUE;
    ~AutoHandle(){if(handle!=INVALID_HANDLE_VALUE && handle!=nullptr)CloseHandle(handle);}
    AutoHandle()=default;
    AutoHandle(const AutoHandle&)=delete;
    AutoHandle& operator=(const AutoHandle&)=delete;
};
void require(bool ok,const char* label) {
    if(!ok)throw std::runtime_error(std::string(label)+" Win32="+
                                   std::to_string(GetLastError()));
}
struct Transfer {
    OVERLAPPED overlapped{};
    std::array<std::uint8_t,kChunkBytes> bytes{};
    bool completed=false;
};
using Requests=std::array<Transfer,kChunks>;
void perform(HANDLE file,HANDLE port,Requests& jobs,bool reading){
    for(DWORD i=0;i<kChunks;i++){
        auto& job=jobs[i];
        job.overlapped={};
        job.overlapped.Offset=i*kChunkBytes;
        job.completed=false;
        BOOL dispatched=reading?
            ReadFile(file,job.bytes.data(),kChunkBytes,nullptr,&job.overlapped):
            WriteFile(file,job.bytes.data(),kChunkBytes,nullptr,&job.overlapped);
        if(!dispatched){
            const DWORD code=GetLastError();
            if(code!=ERROR_IO_PENDING){
                throw std::runtime_error(std::string(reading?"ReadFile":"WriteFile")+
                                         " failed "+std::to_string(code));
            }
        }
    }
    for(DWORD finished=0;finished<kChunks;finished++){
        DWORD bytes=0;
        ULONG_PTR key=0;
        LPOVERLAPPED ptr=nullptr;
        const BOOL success=GetQueuedCompletionStatus(port,&bytes,&key,&ptr,10000);
        require(success && ptr && key==kCompletionKey &&
                bytes==kChunkBytes,"IOCP completion or byte count");
        bool found=false;
        for(auto& job:jobs) {
            if(ptr==&job.overlapped){
                require(!job.completed,"duplicate IOCP completion");
                job.completed=true;
                found=true;
                break;
            }
        }
        require(found,"unknown IOCP OVERLAPPED token");
    }
}
int run(){
    // GetTempFileNameW creates the empty file; FILE_FLAG_DELETE_ON_CLOSE cleans it.
    std::array<wchar_t,MAX_PATH+1> directory{};
    const DWORD n=GetTempPathW(MAX_PATH, directory.data());
    require(n>0 && n<MAX_PATH,"GetTempPathW");
    std::array<wchar_t,MAX_PATH+1> file_name{};
    require(GetTempFileNameW(directory.data(),L"SCI",0,file_name.data())!=0,
            "GetTempFileNameW");
    AutoHandle file{};
    file.handle=CreateFileW(file_name.data(),GENERIC_READ|GENERIC_WRITE|DELETE,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,
        FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_OVERLAPPED|FILE_FLAG_DELETE_ON_CLOSE,
        nullptr);
    if(file.handle==INVALID_HANDLE_VALUE) {
        const auto error=GetLastError();
        DeleteFileW(file_name.data());
        throw std::runtime_error("CreateFileW overlapped="+std::to_string(error));
    }
    AutoHandle port{};
    port.handle=CreateIoCompletionPort(file.handle,nullptr,kCompletionKey,1);
    require(port.handle!=INVALID_HANDLE_VALUE && port.handle!=nullptr,
            "CreateIoCompletionPort");
    Requests write_jobs{};
    Requests read_jobs{};
    for(DWORD i=0;i<kChunks;i++){
        for(DWORD b=0;b<kChunkBytes;b++){
            write_jobs[i].bytes[b]=static_cast<std::uint8_t>(
                (i*43u+b*17u+(b>>4u))&0xffu);
        }
    }
    perform(file.handle,port.handle,write_jobs,false);
    LARGE_INTEGER length{};
    require(GetFileSizeEx(file.handle,&length),"GetFileSizeEx");
    require(length.QuadPart==kTotalBytes,"unexpected file size");
    perform(file.handle,port.handle,read_jobs,true);
    std::uint64_t checksum=0;
    for(DWORD i=0;i<kChunks;i++){
        require(read_jobs[i].bytes==write_jobs[i].bytes,"disk readback mismatch");
        for(const auto value:read_jobs[i].bytes)checksum+=value;
    }
    std::cout<<"{\"kind\":\"native-windows-iocp-overlapped-file\","
             "\"chunk_count\":"<<kChunks<<","
             "\"chunk_bytes\":"<<kChunkBytes<<","
             "\"total_bytes\":"<<kTotalBytes<<","
             "\"completions_expected\":"<<2*kChunks<<","
             "\"readback_checksum\":"<<checksum<<","
             "\"exact_readback_verified\":true,"
             "\"file_cleanup_delete_on_close\":true,"
             "\"directstorage_used\":false,"
             "\"compression_used\":false,"
             "\"game_assets_used\":false}\n";
    return 0;
}
}
int main(){
    try{return run();}
    catch(const std::exception& ex){
        std::cerr<<"native Windows IOCP test FAILED: "<<ex.what()<<"\n";return 1;
    }
}
