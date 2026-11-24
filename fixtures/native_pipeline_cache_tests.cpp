#include "native_pipeline_cache.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace scarlett::pipeline;
namespace {
void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
Bytes read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return Bytes(std::istreambuf_iterator<char>(f),{});}
void write(const std::filesystem::path& p,const Bytes& b){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));f.close();require(bool(f),"fixture write");}
void fix_checksum(Bytes& b){std::uint64_t h=14695981039346656037ull;for(std::size_t i=0;i<b.size()-8;++i){h^=b[i];h*=1099511628211ull;}for(unsigned i=0;i<8;++i)b[b.size()-8+i]=static_cast<std::uint8_t>(h>>(8*i));}
void run(){
    auto path=std::filesystem::temp_directory_path()/("nativex-pso-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));require(std::filesystem::create_directory(path),"temp dir");
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{path};
    auto file=path/"cache.bin",bad=path/"bad.bin";
    const Bytes scope{1,2,3,4},a{1,2},b{1,2,0},v{7,8,9},w{9,8};
    Cache c(scope,256,2);require(c.load(file)==Status::Miss,"missing");
    require(c.put(a,v)==Status::Ok&&c.put(b,w)==Status::Ok,"exact keys");
    require(c.put({3},{9})==Status::Full&&c.put({},v)==Status::Invalid,"limits");
    require(c.find(a)&&*c.find(a)==v&&c.find(b)&&*c.find(b)==w,"keys conflated");
    require(c.put(a,Bytes(250,1))==Status::Full&&*c.find(a)==v,"failed replacement");
    require(c.save(file)==Status::Ok,"save");const auto good=read(file);require(good.size()==c.snapshot_bytes(),"size accounting");
    Cache r(scope,256,2);require(r.load(file)==Status::Ok&&r.size()==2&&*r.find(a)==v&&*r.find(b)==w,"reopen");
    Cache stale({1,2,3,5},256,2);require(stale.put({9},v)==Status::Ok&&stale.load(file)==Status::ScopeMismatch&&stale.find({9}),"scope mismatch");
    Cache small(scope,28,2);require(small.load(file)==Status::Full,"file cap");
    Cache few(scope,256,1);require(few.load(file)==Status::Full,"count cap");
    // Each failed load must leave the previously published map untouched.
    auto reject=[&](const Bytes& x){write(bad,x);require(r.load(bad)==Status::Corrupt&&r.size()==2&&*r.find(a)==v&&*r.find(b)==w,"corrupt file accepted or map changed");};
    for(std::size_t i=0;i<good.size();++i){auto x=good;x[i]^=0x80;reject(x);}
    for(std::size_t i=0;i<good.size();++i)reject(Bytes(good.begin(),good.begin()+static_cast<std::ptrdiff_t>(i)));
    auto x=good;x[20]=0xff;x[21]=0xff;x[22]=0xff;x[23]=0xff;fix_checksum(x);reject(x); // key length
    x=good;x[24]=0xff;x[25]=0xff;x[26]=0xff;x[27]=0xff;fix_checksum(x);reject(x); // blob length
    x=good;x[12]=1;fix_checksum(x);reject(x); // extra trailing record
    x=good;x[8]=0;fix_checksum(x);reject(x); // empty scope
    // Identical canonical keys with different blobs, valid checksum.
    Cache dup(scope,256,2);require(dup.put({1},{5})==Status::Ok&&dup.put({2},{6})==Status::Ok&&dup.save(bad)==Status::Ok,"duplicate fixture");x=read(bad);x[38]=1;fix_checksum(x);reject(x);
    require(r.erase(a)&&!r.erase(a)&&r.put(b,{4})==Status::Ok&&r.save(file)==Status::Ok,"overwrite snapshot");
    Cache newer(scope,256,2);require(newer.load(file)==Status::Ok&&!newer.find(a)&&newer.find(b)&&*newer.find(b)==Bytes{4},"overwrite reload");
    require(r.save(path)==Status::IoError&&std::filesystem::is_directory(path),"directory replacement");
    // Distinct snapshots and independent exact-key map oracle.
    Cache many(scope,8192,64);std::map<Bytes,Bytes> oracle;std::mt19937 rng(20261124);unsigned reopened=0;
    for(unsigned i=0;i<2000;++i){Bytes k{static_cast<std::uint8_t>(rng()%64)},val(static_cast<std::size_t>(rng()%64+1),static_cast<std::uint8_t>(rng()%256));if(rng()%3==0){require(many.erase(k)==(oracle.erase(k)!=0),"erase oracle");}else {require(many.put(k,val)==Status::Ok,"put oracle");oracle[k]=val;}
        if(i%37==0){require(many.save(file)==Status::Ok,"repeated save");Cache round(scope,8192,64);require(round.load(file)==Status::Ok&&round.size()==oracle.size(),"repeated reload");for(const auto& kv:oracle)require(round.find(kv.first)&&*round.find(kv.first)==kv.second,"roundtrip oracle");++reopened;}
    }
    std::cout<<"{\"snapshot_bytes\":"<<good.size()<<",\"corruption_controls\":"<<good.size()*2+5<<",\"map_operations\":2000,\"reopens\":"<<reopened<<",\"errors\":0}\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
