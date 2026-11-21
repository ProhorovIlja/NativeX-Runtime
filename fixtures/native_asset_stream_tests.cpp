// Source-only deterministic portable asset stream exercise; no game assets.
#include "native_asset_stream.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using scarlett::stream::Reader;
using scarlett::stream::Request;
using scarlett::stream::Result;
using scarlett::stream::Status;
namespace {
void check(bool condition,const char* reason){if(!condition)throw std::runtime_error(reason);}
struct TestDir {
    std::filesystem::path path;
    TestDir() {
        const auto now=std::chrono::steady_clock::now().time_since_epoch().count();
        path=std::filesystem::temp_directory_path()/("scarlett-native-stream-"+std::to_string(now));
        if(!std::filesystem::create_directory(path))throw std::runtime_error("temp path collision");
    }
    ~TestDir() {std::error_code error;std::filesystem::remove_all(path,error);}
};
void run() {
    const std::vector<std::uint8_t> abc={'a','b','c'};
    check(scarlett::stream::fnv1a64(abc)==0xe71fa2190541574bull,"independent FNV-1a known vector");
    TestDir root;
    const auto file=root.path/"test assets with spaces.bin";
    std::vector<std::uint8_t> payload(2u*1024u*1024u);
    for(std::size_t i=0;i<payload.size();i++)
        payload[i]=static_cast<std::uint8_t>((i*17u+3u)%251u);
    {
        std::ofstream out(file,std::ios::binary|std::ios::trunc);
        check(out.is_open(),"could not create synthetic asset");
        out.write(reinterpret_cast<const char*>(payload.data()),
                  static_cast<std::streamsize>(payload.size()));
        out.flush();
        check(out.good(),"synthetic asset was not flushed");
    }
    Reader reader(4,128);
    std::vector<std::future<Result>> futures;
    for(std::size_t i=0;i<64;i++)
        futures.push_back(reader.submit(Request{file,i*8192u,8192u,{}}));
    for(std::size_t i=0;i<futures.size();i++) {
        const auto got=futures[i].get();
        check(got.status==Status::Ok && got.bytes.size()==8192u,"async read status or length");
        for(std::size_t j=0;j<got.bytes.size();j++)
            check(got.bytes[j]==payload[i*8192u+j],"async read data mismatch");
        check(got.checksum==scarlett::stream::fnv1a64(got.bytes),"read hash mismatch");
    }
    check(reader.submit(Request{file,0,scarlett::stream::MaxRequestBytes+1u,{}}).get().status
          ==Status::InvalidRange,"oversized request accepted");
    check(reader.submit(Request{file,static_cast<std::uint64_t>(payload.size()),0,{}}).get().status
          ==Status::Ok,"zero-length read at EOF");
    check(reader.submit(Request{file,static_cast<std::uint64_t>(payload.size()),1,{}}).get().status
          ==Status::InvalidRange,"past EOF accepted");
    check(reader.submit(Request{file,UINT64_MAX,1,{}}).get().status
          ==Status::InvalidRange,"overflowed offset accepted");
    check(reader.submit(Request{root.path/"not-found.rpf",0,12,{}}).get().status
          ==Status::FileError,"nonexistent asset accepted");
    const auto token=std::make_shared<std::atomic_bool>(true);
    const auto cancelled=reader.submit(Request{file,0,4096,token}).get();
    check(cancelled.status==Status::Cancelled && cancelled.bytes.empty(),
          "pre-cancelled read leaked data");
    {
        // Destructor must drain accepted work and fulfill every future.
        std::vector<std::future<Result>> drain;
        {
            Reader ephemeral(3,128);
            for(std::size_t i=0;i<24;i++)
                drain.push_back(ephemeral.submit(Request{file,i*2048u,2048u,{}}));
        }
        for(auto& future:drain)
            check(future.get().status==Status::Ok,"thread shutdown lost accepted reads");
    }
    bool invalid=false;
    try{Reader not_allowed(0,1);}catch(const std::invalid_argument&){invalid=true;}
    check(invalid,"zero-worker configuration accepted");
}
}
int main() {
    try{run();}
    catch(const std::exception& e){
        std::cerr<<"portable native streaming FAIL: "<<e.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"source-built-native-asset-streaming\","
             <<"\"os_native_threads\":true,\"deterministic_chunks\":64,"
             <<"\"shutdown_drained\":true,\"overflow_checked\":true,"
             <<"\"game_data_used\":false,\"directstorage_executed\":false,"
             <<"\"io_uring_executed\":false}\n";
    return 0;
}
