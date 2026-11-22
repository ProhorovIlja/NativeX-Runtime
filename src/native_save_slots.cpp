#include "native_save_slots.hpp"
#include <array>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace scarlett::save {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'S','C','S','A','V','E','0','1'}};
constexpr std::size_t HeaderSize=40;
constexpr std::uint32_t Version=1u;
constexpr std::uint64_t HashBasis=14695981039346656037ull;
constexpr std::uint64_t HashPrime=1099511628211ull;
enum class Slot : std::uint8_t { A,B };
std::filesystem::path location(const std::filesystem::path& dir,Slot slot) {
    return dir/(slot==Slot::A?"slot-a.scp":"slot-b.scp");
}
void put32(std::vector<std::uint8_t>& bytes,std::uint32_t n) {
    for(int i=0;i<4;i++)bytes.push_back(static_cast<std::uint8_t>((n>>(i*8))&255u));
}
void put64(std::vector<std::uint8_t>& bytes,std::uint64_t n) {
    for(int i=0;i<8;i++)bytes.push_back(static_cast<std::uint8_t>((n>>(i*8))&255u));
}
std::uint32_t get32(const std::uint8_t* p) {
    std::uint32_t result=0;
    for(int i=0;i<4;i++)result|=static_cast<std::uint32_t>(p[i])<<(i*8);
    return result;
}
std::uint64_t get64(const std::uint8_t* p) {
    std::uint64_t result=0;
    for(int i=0;i<8;i++)result|=static_cast<std::uint64_t>(p[i])<<(i*8);
    return result;
}
std::uint64_t checksum(const std::uint8_t* b,std::size_t n,std::uint64_t hash=HashBasis) {
    for(std::size_t i=0;i<n;i++) {
        hash^=b[i];
        hash*=HashPrime;
    }
    return hash;
}
std::vector<std::uint8_t> encode(std::uint64_t generation,
                                   const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(HeaderSize+payload.size());
    bytes.insert(bytes.end(),Magic.begin(),Magic.end());
    put32(bytes,Version);
    put32(bytes,0u);
    put64(bytes,generation);
    put64(bytes,static_cast<std::uint64_t>(payload.size()));
    std::uint64_t hash=checksum(bytes.data(),bytes.size());
    if(!payload.empty())hash=checksum(payload.data(),payload.size(),hash);
    put64(bytes,hash);
    bytes.insert(bytes.end(),payload.begin(),payload.end());
    return bytes;
}
struct SlotRead {
    Status status=Status::NotFound;
    Snapshot snapshot{};
};
SlotRead read_slot(const std::filesystem::path& file) {
    std::error_code ec;
    const bool exists=std::filesystem::exists(file,ec);
    if(ec)return {Status::IoError,{}};
    if(!exists)return {Status::NotFound,{}};
    const auto len=std::filesystem::file_size(file,ec);
    if(ec)return {Status::IoError,{}};
    if(len<HeaderSize || len>HeaderSize+MaxPayloadBytes)return {Status::Corrupt,{}};
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(len));
    std::ifstream input(file,std::ios::binary);
    if(!input)return {Status::IoError,{}};
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if(!input || input.gcount()!=static_cast<std::streamsize>(bytes.size()))
        return {Status::Corrupt,{}};
    if(std::memcmp(bytes.data(),Magic.data(),Magic.size())!=0 ||
       get32(bytes.data()+8)!=Version || get32(bytes.data()+12)!=0 ||
       get64(bytes.data()+16)==0 ||
       get64(bytes.data()+24)!=bytes.size()-HeaderSize)return {Status::Corrupt,{}};
    const std::uint64_t on_disk_hash=get64(bytes.data()+32);
    std::uint64_t hash=checksum(bytes.data(),32);
    hash=checksum(bytes.data()+HeaderSize,bytes.size()-HeaderSize,hash);
    if(hash!=on_disk_hash)return {Status::Corrupt,{}};
    Snapshot snapshot{};
    snapshot.generation=get64(bytes.data()+16);
    snapshot.bytes.assign(bytes.begin()+static_cast<std::ptrdiff_t>(HeaderSize),bytes.end());
    return {Status::Ok,std::move(snapshot)};
}
struct Latest {
    Status status=Status::NotFound;
    Snapshot snapshot{};
    Slot source=Slot::B; // If no committed generation, first target is A.
};
Latest inspect(const std::filesystem::path& directory) {
    auto a=read_slot(location(directory,Slot::A));
    auto b=read_slot(location(directory,Slot::B));
    // If either file cannot be inspected, the unreadable slot could hold
    // the newest generation. Never silently fall back to a stale snapshot
    // or accept a write based on incomplete generation evidence.
    if(a.status==Status::IoError || b.status==Status::IoError)
        return {Status::IoError,{},Slot::B};
    if(a.status==Status::Ok && (b.status!=Status::Ok ||
                               a.snapshot.generation>=b.snapshot.generation))
        return {Status::Ok,std::move(a.snapshot),Slot::A};
    if(b.status==Status::Ok)return {Status::Ok,std::move(b.snapshot),Slot::B};
    if(a.status==Status::NotFound && b.status==Status::NotFound)
        return {Status::NotFound,{},Slot::B};
    return {Status::Corrupt,{},Slot::B};
}
#ifdef _WIN32
bool write_sync(const std::filesystem::path& file,
                const std::vector<std::uint8_t>& bytes) {
    HANDLE handle=CreateFileW(file.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,nullptr);
    if(handle==INVALID_HANDLE_VALUE)return false;
    bool ok=true;
    std::size_t offset=0;
    while(offset<bytes.size()) {
        const DWORD n=static_cast<DWORD>(bytes.size()-offset); // <=2 MiB
        DWORD written=0;
        if(!WriteFile(handle,bytes.data()+offset,n,&written,nullptr) || written==0){
            ok=false;break;
        }
        offset+=written;
    }
    if(ok)ok=FlushFileBuffers(handle)!=FALSE;
    if(!CloseHandle(handle))ok=false;
    return ok;
}
bool publish(const std::filesystem::path& stage,const std::filesystem::path& target) {
    return MoveFileExW(stage.c_str(),target.c_str(),
                       MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}
bool sync_directory(const std::filesystem::path&) {return true;}
#else
bool write_sync(const std::filesystem::path& file,
                const std::vector<std::uint8_t>& bytes) {
    const int fd=::open(file.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);
    if(fd<0)return false;
    bool ok=true;
    std::size_t offset=0;
    while(offset<bytes.size()){
        const ssize_t n=::write(fd,bytes.data()+offset,bytes.size()-offset);
        if(n<0 && errno==EINTR)continue;
        if(n<=0){ok=false;break;}
        offset+=static_cast<std::size_t>(n);
    }
    if(ok && ::fsync(fd)!=0)ok=false;
    if(::close(fd)!=0)ok=false;
    return ok;
}
bool publish(const std::filesystem::path& stage,const std::filesystem::path& target) {
    return ::rename(stage.c_str(),target.c_str())==0;
}
bool sync_directory(const std::filesystem::path& directory) {
    const int fd=::open(directory.c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(fd<0)return false;
    const bool ok=::fsync(fd)==0;
    const bool closed=::close(fd)==0;
    return ok&&closed;
}
#endif
void discard(const std::filesystem::path& file) {
    std::error_code ignored;
    std::filesystem::remove(file,ignored);
}
}
Status Store::load(Snapshot& snapshot) const {
    const Latest latest=inspect(directory_);
    if(latest.status==Status::Ok)
        snapshot=latest.snapshot;
    else
        snapshot=Snapshot{};
    return latest.status;
}
Status Store::commit(const std::vector<std::uint8_t>& bytes,
                     std::uint64_t expected_generation,Fault fault) {
    if(bytes.size()>MaxPayloadBytes)return Status::TooLarge;
    const Latest previous=inspect(directory_);
    if(previous.status!=Status::Ok && previous.status!=Status::NotFound)
        return previous.status;
    const auto actual=previous.status==Status::Ok?previous.snapshot.generation:0u;
    if(actual!=expected_generation)return Status::Conflict;
    if(actual==std::numeric_limits<std::uint64_t>::max())return Status::InvalidState;
    std::error_code ec;
    std::filesystem::create_directories(directory_,ec);
    if(ec)return Status::IoError;
    const auto stage=directory_/"save-stage.tmp";
    // Single-owner contract: a stale stage left by a previous process is discarded.
    discard(stage);
    const auto encoded=encode(actual+1,bytes);
    if(!write_sync(stage,encoded)) {
        discard(stage);
        return Status::IoError;
    }
    if(fault==Fault::BeforePublish) {
        discard(stage);
        return Status::Interrupted;
    }
    const Slot target=previous.source==Slot::A?Slot::B:Slot::A;
    if(!publish(stage,location(directory_,target))) {
        discard(stage);
        return Status::IoError;
    }
    // Simulated failure after name replacement: new generation may be visible,
    // but durability across an actual power cut has not been demonstrated.
    if(fault==Fault::AfterPublish)return Status::Interrupted;
    if(!sync_directory(directory_))return Status::IoError;
    return Status::Ok;
}
const char* status_text(Status value) noexcept {
    switch(value){
        case Status::Ok:return "ok";
        case Status::NotFound:return "not-found";
        case Status::Corrupt:return "corrupt";
        case Status::IoError:return "io-error";
        case Status::TooLarge:return "too-large";
        case Status::Conflict:return "generation-conflict";
        case Status::InvalidState:return "invalid-state";
        case Status::Interrupted:return "injected-interruption";
    }
    return "unknown";
}
}
