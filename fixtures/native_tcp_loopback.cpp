// Original native Windows Winsock/Linux POSIX loopback TCP fixture.
// Localhost-only child process transport, bounded framing, integrity and rejection.
// NOT multiplayer, NAT, matchmaking, Xbox Live networking or a GTA VI protocol.
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cwchar>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
namespace {
constexpr std::size_t PayloadBytes=256u*1024u;
constexpr std::size_t HeaderBytes=12u;
constexpr std::size_t ChunkBytes=4096u;
constexpr std::uint64_t Offset=14695981039346656037ull;
constexpr std::uint64_t Prime=1099511628211ull;
#ifdef _WIN32
using NativeSocket=SOCKET;
constexpr NativeSocket Invalid=INVALID_SOCKET;
int close_socket(NativeSocket s){return closesocket(s);}
struct Winsock {
    Winsock(){
        WSADATA info{};
        if(WSAStartup(MAKEWORD(2,2),&info)!=0)
            throw std::runtime_error("WSAStartup failed");
    }
    ~Winsock(){WSACleanup();}
};
#else
using NativeSocket=int;
constexpr NativeSocket Invalid=-1;
int close_socket(NativeSocket s){return close(s);}
struct Winsock {};
#endif
struct Socket {
    NativeSocket fd=Invalid;
    ~Socket(){if(fd!=Invalid)close_socket(fd);}
    Socket()=default;
    Socket(const Socket&)=delete;
    Socket& operator=(const Socket&)=delete;
    Socket(Socket&& other) noexcept : fd(other.fd){other.fd=Invalid;}
    Socket& operator=(Socket&& other) noexcept {
        if(this!=&other){
            if(fd!=Invalid)close_socket(fd);
            fd=other.fd;
            other.fd=Invalid;
        }
        return *this;
    }
};
std::uint8_t pattern(std::size_t index,bool reply) {
    const auto p=static_cast<std::uint8_t>((index*29u+7u)%251u);
    return reply?static_cast<std::uint8_t>(p^0x5au):p;
}
std::uint64_t checksum(const std::vector<std::uint8_t>& data) {
    std::uint64_t value=Offset;
    for(const auto byte:data){value^=byte;value*=Prime;}
    return value;
}
bool match(const std::vector<std::uint8_t>& bytes,bool reply){
    if(bytes.size()!=PayloadBytes)return false;
    for(std::size_t i=0;i<bytes.size();i++)
        if(bytes[i]!=pattern(i,reply))return false;
    return true;
}
void put32(std::uint8_t* to,std::uint32_t n) {
    for(int i=3;i>=0;i--){to[i]=static_cast<std::uint8_t>(n&255u);n>>=8;}
}
void put64(std::uint8_t* to,std::uint64_t n) {
    for(int i=7;i>=0;i--){to[i]=static_cast<std::uint8_t>(n&255u);n>>=8;}
}
std::uint32_t get32(const std::uint8_t* p) {
    std::uint32_t n=0;
    for(int i=0;i<4;i++)n=(n<<8)|p[i];
    return n;
}
std::uint64_t get64(const std::uint8_t* p) {
    std::uint64_t n=0;
    for(int i=0;i<8;i++)n=(n<<8)|p[i];
    return n;
}
bool send_exact(NativeSocket s,const std::uint8_t* data,std::size_t n){
    std::size_t sent=0;
    while(sent<n){
        const int size=static_cast<int>((n-sent)<ChunkBytes?(n-sent):ChunkBytes);
#ifdef _WIN32
        const int wrote=send(s,reinterpret_cast<const char*>(data+sent),size,0);
#else
        const int wrote=static_cast<int>(send(s,data+sent,
                                               static_cast<std::size_t>(size),MSG_NOSIGNAL));
#endif
        if(wrote<=0)return false;
        sent+=static_cast<std::size_t>(wrote);
    }
    return true;
}
bool recv_exact(NativeSocket s,std::uint8_t* bytes,std::size_t n){
    std::size_t pos=0;
    while(pos<n){
        const int wanted=static_cast<int>((n-pos)<ChunkBytes?(n-pos):ChunkBytes);
#ifdef _WIN32
        const int received=recv(s,reinterpret_cast<char*>(bytes+pos),wanted,0);
#else
        const int received=static_cast<int>(recv(s,bytes+pos,
                                                static_cast<std::size_t>(wanted),0));
#endif
        if(received<=0)return false;
        pos+=static_cast<std::size_t>(received);
    }
    return true;
}
bool send_packet(NativeSocket s,const std::vector<std::uint8_t>& data){
    if(data.size()>PayloadBytes)return false;
    std::array<std::uint8_t,HeaderBytes> header{};
    put32(header.data(),static_cast<std::uint32_t>(data.size()));
    put64(header.data()+4,checksum(data));
    return send_exact(s,header.data(),header.size()) &&
           (data.empty() || send_exact(s,data.data(),data.size()));
}
enum class Receive { Ok, Oversized, Invalid, IoError };
Receive receive_packet(NativeSocket s,std::vector<std::uint8_t>& out) {
    std::array<std::uint8_t,HeaderBytes> header{};
    if(!recv_exact(s,header.data(),header.size()))return Receive::IoError;
    const auto length=get32(header.data());
    if(length>PayloadBytes)return Receive::Oversized; // fail BEFORE allocation
    std::vector<std::uint8_t> candidate(length);
    if(length && !recv_exact(s,candidate.data(),length))return Receive::IoError;
    if(checksum(candidate)!=get64(header.data()+4))return Receive::Invalid;
    out=std::move(candidate);
    return Receive::Ok;
}
bool timeouts(NativeSocket socket){
#ifdef _WIN32
    const DWORD ms=5000;
    return setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,
                      reinterpret_cast<const char*>(&ms),sizeof(ms))==0 &&
           setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,
                      reinterpret_cast<const char*>(&ms),sizeof(ms))==0;
#else
    const timeval t{5,0};
    return setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,&t,sizeof(t))==0 &&
           setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof(t))==0;
#endif
}
int child(std::uint16_t port,bool oversized){
    [[maybe_unused]] Winsock sockets{};
    Socket connection{};
    connection.fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(connection.fd==Invalid||!timeouts(connection.fd))return 101;
    sockaddr_in remote{};
    remote.sin_family=AF_INET;
    remote.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    remote.sin_port=htons(port);
    if(connect(connection.fd,reinterpret_cast<const sockaddr*>(&remote),
               sizeof(remote))!=0)return 102;
    if(oversized){
        std::array<std::uint8_t,HeaderBytes> header{};
        put32(header.data(),static_cast<std::uint32_t>(PayloadBytes+1u));
        put64(header.data()+4,0);
        return send_exact(connection.fd,header.data(),header.size())?0:103;
    }
    std::vector<std::uint8_t> payload(PayloadBytes);
    for(std::size_t i=0;i<payload.size();i++)payload[i]=pattern(i,false);
    if(!send_packet(connection.fd,payload))return 104;
    std::vector<std::uint8_t> response{};
    if(receive_packet(connection.fd,response)!=Receive::Ok ||
       !match(response,true))return 105;
    return 0;
}
#ifdef _WIN32
struct Child {
    HANDLE process=nullptr;
    Child()=default;
    Child(const Child&)=delete;
    Child& operator=(const Child&)=delete;
    Child(Child&& other) noexcept : process(other.process){other.process=nullptr;}
    Child& operator=(Child&& other) noexcept {
        if(this!=&other){
            if(process)CloseHandle(process);
            process=other.process;other.process=nullptr;
        }
        return *this;
    }
    ~Child(){if(process)CloseHandle(process);}
};
Child launch_child(const wchar_t* mode,std::uint16_t port){
    std::array<wchar_t,32768> self{};
    const DWORD len=GetModuleFileNameW(nullptr,self.data(),
                                      static_cast<DWORD>(self.size()));
    if(!len||len>=self.size()-1u)throw std::runtime_error("self exe path missing");
    std::wstring command=L"\""+std::wstring(self.data(),len)+
                         L"\" "+mode+L" "+std::to_wstring(port);
    STARTUPINFOW start{};start.cb=sizeof(start);
    PROCESS_INFORMATION proc{};
    if(!CreateProcessW(nullptr,command.data(),nullptr,nullptr,FALSE,
                       CREATE_NO_WINDOW,nullptr,nullptr,&start,&proc))
        throw std::runtime_error("CreateProcessW client failed");
    CloseHandle(proc.hThread);
    Child childProcess{};
    childProcess.process=proc.hProcess;
    return childProcess;
}
bool wait_child(Child& process){
    const DWORD waited=WaitForSingleObject(process.process,10000);
    if(waited!=WAIT_OBJECT_0){
        TerminateProcess(process.process,199);
        WaitForSingleObject(process.process,2000);
        return false;
    }
    DWORD code=1;
    return GetExitCodeProcess(process.process,&code)!=0 && code==0;
}
#else
struct Child {
    pid_t pid=-1;
};
Child launch_child(bool oversized,std::uint16_t port){
    const pid_t pid=fork();
    if(pid<0)throw std::runtime_error("fork client failed");
    if(pid==0)_exit(child(port,oversized));
    return {pid};
}
bool wait_child(Child& process){
    int status=0;
    for(int i=0;i<1000;i++){
        const auto waited=waitpid(process.pid,&status,WNOHANG);
        if(waited==process.pid)
            return WIFEXITED(status)&&WEXITSTATUS(status)==0;
        if(waited<0)return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    kill(process.pid,SIGKILL);
    waitpid(process.pid,&status,0);
    return false;
}
#endif
Socket accept_bounded(NativeSocket listener){
    fd_set read_set{};
    FD_ZERO(&read_set);
    FD_SET(listener,&read_set);
    timeval limit{5,0};
#ifdef _WIN32
    const int ready=select(0,&read_set,nullptr,nullptr,&limit);
#else
    const int ready=select(listener+1,&read_set,nullptr,nullptr,&limit);
#endif
    if(ready!=1)throw std::runtime_error("local TCP accept timed out");
    Socket connected{};
    connected.fd=accept(listener,nullptr,nullptr);
    if(connected.fd==Invalid||!timeouts(connected.fd))
        throw std::runtime_error("local TCP accept/socket timeout setup failed");
    return connected;
}
int parent(){
    [[maybe_unused]] Winsock sockets{};
    Socket listener{};
    listener.fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(listener.fd==Invalid||!timeouts(listener.fd))
        throw std::runtime_error("TCP localhost socket create failed");
    sockaddr_in local{};
    local.sin_family=AF_INET;
    local.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    local.sin_port=0;
    if(bind(listener.fd,reinterpret_cast<const sockaddr*>(&local),sizeof(local))!=0 ||
       listen(listener.fd,2)!=0)
        throw std::runtime_error("bind/listen loopback failed");
#ifdef _WIN32
    int address_size=sizeof(local);
#else
    socklen_t address_size=sizeof(local);
#endif
    if(getsockname(listener.fd,reinterpret_cast<sockaddr*>(&local),&address_size)!=0)
        throw std::runtime_error("getsockname failed");
    const auto port=ntohs(local.sin_port);
    if(port==0 || ntohl(local.sin_addr.s_addr)!=INADDR_LOOPBACK)
        throw std::runtime_error("socket not bound to private 127.0.0.1");

#ifdef _WIN32
    Child good=launch_child(L"--client",port);
#else
    Child good=launch_child(false,port);
#endif
    {
        Socket connection=accept_bounded(listener.fd);
        std::vector<std::uint8_t> payload{};
        if(receive_packet(connection.fd,payload)!=Receive::Ok ||
           !match(payload,false))
            throw std::runtime_error("real TCP framed request integrity failure");
        for(std::size_t i=0;i<payload.size();i++)payload[i]=pattern(i,true);
        if(!send_packet(connection.fd,payload))
            throw std::runtime_error("TCP response send failed");
    }
    if(!wait_child(good))
        throw std::runtime_error("TCP client did not verify response");
#ifdef _WIN32
    Child bad=launch_child(L"--oversized",port);
#else
    Child bad=launch_child(true,port);
#endif
    {
        Socket connection=accept_bounded(listener.fd);
        std::vector<std::uint8_t> invalid{};
        if(receive_packet(connection.fd,invalid)!=Receive::Oversized ||
           !invalid.empty())
            throw std::runtime_error("oversized TCP frame not rejected before allocation");
    }
    if(!wait_child(bad))
        throw std::runtime_error("oversized-frame child failed");
    std::cout<<"{\"kind\":\"native-windows-linux-local-tcp\","
             "\"only_loopback_bound\":true,"
             "\"distinct_client_processes\":2,"
             "\"payload_bytes_each_direction\":"<<PayloadBytes<<","
             "\"bounded_frame_integrity_verified\":true,"
             "\"oversized_frame_rejected_before_allocation\":true,"
             "\"external_network_or_nat_tested\":false,"
             "\"game_netcode_tested\":false}\n";
    return 0;
}
}
#ifdef _WIN32
int wmain(int argc,wchar_t** argv){
    try{
        if(argc==3 && (std::wcscmp(argv[1],L"--client")==0 ||
                       std::wcscmp(argv[1],L"--oversized")==0)){
            wchar_t* end=nullptr;
            const unsigned long parsed=std::wcstoul(argv[2],&end,10);
            if(end==argv[2]||*end!=L'\0'||parsed==0||parsed>65535)return 2;
            return child(static_cast<std::uint16_t>(parsed),
                         std::wcscmp(argv[1],L"--oversized")==0);
        }
        if(argc!=1)return 2;
        return parent();
    }catch(const std::exception& e){
        std::cerr<<"native TCP test FAILED: "<<e.what()<<"\n";return 1;
    }
}
#else
int main(int argc,char** /*argv*/){
    try{
        if(argc!=1)return 2;
        return parent();
    }catch(const std::exception& e){
        std::cerr<<"native TCP test FAILED: "<<e.what()<<"\n";return 1;
    }
}
#endif
