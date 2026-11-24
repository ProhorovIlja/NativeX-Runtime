// Original native cache fixture with independently generated data.
#include "native_asset_cache.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace scarlett::stream;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
Result payload(std::size_t n,std::uint8_t byte){Result r{Status::Ok,std::vector<std::uint8_t>(n,byte),0};r.checksum=fnv1a64(r.bytes);return r;}
struct Temp {
    std::filesystem::path dir;
    Temp(){dir=std::filesystem::temp_directory_path()/("nativex-cache-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));require(std::filesystem::create_directory(dir),"temp collision");}
    ~Temp(){std::error_code ec;std::filesystem::remove_all(dir,ec);}
};
void controls(){
    AssetCache cache(8,2);
    CacheKey a{"a",1,0,4},b{"b",1,0,4},c{"c",1,0,4};
    require(!cache.find(a),"initial miss");
    require(cache.insert(a,payload(4,1))==CacheInsert::Inserted,"insert a");
    require(cache.insert(b,payload(4,2))==CacheInsert::Inserted,"insert b");
    auto held=cache.find(a);require(held&&held->bytes[0]==1,"hit a");
    require(cache.insert(c,payload(4,3))==CacheInsert::Inserted,"insert c");
    require(!cache.find(b)&&cache.find(a)&&cache.find(c),"LRU order");
    require(cache.stats().resident_bytes==8&&cache.stats().evictions==1,"byte eviction accounting");
    require(cache.insert(a,payload(4,1))==CacheInsert::AlreadyPresent,"duplicate not reused");
    require(cache.insert(a,payload(4,9))==CacheInsert::Conflict,"conflicting generation accepted");
    require(cache.find(a)->bytes[0]==1,"conflict changed result");
    require(!cache.find({"a",2,0,4}),"generation incorrectly aliased");
    require(!cache.find({"a",1,1,4}),"offset incorrectly aliased");
    require(!cache.find({"a",1,0,3}),"length incorrectly aliased");
    require(!cache.find({"A",1,0,4}),"identity incorrectly folded");
    require(cache.insert({"a",2,0,4},payload(4,8))==CacheInsert::Inserted,"new generation");
    require(cache.invalidate("a")==2&&!cache.find(a),"invalidate all generations");
    require(held->bytes[0]==1,"eviction destroyed held result");
    auto bad=payload(4,1);bad.checksum=0;
    require(cache.insert(a,std::move(bad))==CacheInsert::Invalid,"bad checksum");
    require(cache.insert(a,{Status::Cancelled,{},0})==CacheInsert::Invalid,"cancel cached");
    require(cache.insert(a,payload(3,1))==CacheInsert::Invalid,"wrong length");
    require(cache.insert({"",1,0,4},payload(4,1))==CacheInsert::Invalid,"empty identity");
    require(cache.insert({"a",1,UINT64_MAX,4},payload(4,1))==CacheInsert::Invalid,"offset overflow");
    require(cache.insert({"large",1,0,9},payload(9,1))==CacheInsert::TooLarge,"oversize accepted");
    cache.clear();require(cache.stats().entries==0&&cache.stats().resident_bytes==0,"clear accounting");
    AssetCache zero(8,1);
    require(zero.insert({"a",1,0,0},payload(0,0))==CacheInsert::Inserted,"empty result");
    require(zero.insert({"b",1,0,0},payload(0,0))==CacheInsert::Inserted,"empty result 2");
    require(zero.stats().entries==1&&zero.stats().evictions==1,"entry count bound");
    bool threw=false;try{AssetCache invalid(0);}catch(const std::invalid_argument&){threw=true;}require(threw,"zero budget");
}
void integrated(){
    Temp temp;const auto path=temp.dir/"source.bin";
    constexpr std::size_t chunk=4096;
    std::vector<std::uint8_t> source(chunk*16);
    for(std::size_t i=0;i<source.size();++i)source[i]=static_cast<std::uint8_t>((i*19u+7u)%251u);
    {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(source.data()),static_cast<std::streamsize>(source.size()));require(bool(f),"write fixture");}
    Reader reader(2,16,chunk*16);
    AssetCache cache(chunk*16,16);
    unsigned reads=0;
    for(unsigned i=0;i<1024;++i){
        const auto offset=static_cast<std::size_t>(i%16)*chunk;
        CacheKey key{"source",1,offset,chunk};
        auto found=cache.find(key);
        if(!found){
            ++reads;
            auto result=reader.submit({path,offset,chunk,{}}).get();
            require(cache.insert(key,std::move(result))==CacheInsert::Inserted,"reader cache insert");
            found=cache.find(key);
        }
        require(found&&std::equal(found->bytes.begin(),found->bytes.end(),source.begin()+static_cast<std::ptrdiff_t>(offset)),"cache byte mismatch");
    }
    require(reads==16&&cache.stats().misses==16,"repeated request reuse");
    require(cache.stats().resident_bytes==chunk*16,"resident bytes");
    std::cout<<"{\"logical_requests\":1024,\"reader_submissions\":"<<reads<<",\"avoided_submissions\":"<<1024-reads<<"}\n";
}
void concurrent(){
    AssetCache cache(4096,32);
    std::atomic<unsigned> errors{0};
    std::vector<std::thread> threads;
    for(unsigned t=0;t<8;++t)threads.emplace_back([&,t]{
        for(unsigned i=0;i<512;++i){
            const auto k=(i+t)%64;
            CacheKey key{"asset",1,k*128u,128};
            auto result=cache.find(key);
            if(!result){
                const auto status=cache.insert(key,payload(128,static_cast<std::uint8_t>(k)));
                if(status!=CacheInsert::Inserted&&status!=CacheInsert::AlreadyPresent)++errors;
            } else if(result->bytes.size()!=128||result->bytes.front()!=k)++errors;
            if(i%127==0)cache.invalidate("asset");
            const auto snap=cache.stats();
            if(snap.entries>snap.entry_limit||snap.resident_bytes>snap.byte_limit||snap.entries*128!=snap.resident_bytes)++errors;
        }
    });
    for(auto& t:threads)t.join();
    require(errors==0,"concurrent invariant failure");
    std::cout<<"{\"concurrent_attempts\":4096,\"invariant_errors\":"<<errors<<"}\n";
}
}
int main(){try{controls();integrated();concurrent();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
