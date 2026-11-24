#include "native_pipeline_scope.hpp"
#include <chrono>
#include <iostream>
#include <set>
#include <stdexcept>
using namespace scarlett::pipeline;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void run(){
    ScopeIdentity base{};base.vendor=0x10de;base.device=0x1234;base.subsystem=0x4567;base.revision=1;
    base.luid_low=77;base.luid_high=2;base.node_count=1;base.driver=0x0001000200030004ull;
    base.d3d12=0x000a000000010002ull;base.dxgi=base.d3d12;base.compiler=base.d3d12;
    base.producer={'f','i','x',1};
    const auto identity=compatibility_scope(base);require(!identity.empty()&&identity==compatibility_scope(base),"deterministic scope");
    // Explicit schema oracle: fixed header and little-endian numeric fields.
    const Bytes expected{'N','X','S','C',1,0,0,0,
        0xde,0x10,0,0, 0x34,0x12,0,0, 0x67,0x45,0,0, 1,0,0,0,
        77,0,0,0, 2,0,0,0, 1,0,0,0, 0,0,
        4,0,3,0,2,0,1,0, 2,0,1,0,0,0,10,0,
        2,0,1,0,0,0,10,0, 2,0,1,0,0,0,10,0,
        0,0,0,0,0,0,0,0, 4,0,0,0,'f','i','x',1};
    require(identity==expected,"serialized scope schema mismatch");
    std::set<Bytes> distinct{identity};
    auto changed=[&](ScopeIdentity x){auto s=compatibility_scope(x);require(!s.empty()&&distinct.insert(s).second,"scope collision");};
    auto x=base;++x.vendor;changed(x);x=base;++x.device;changed(x);x=base;++x.subsystem;changed(x);x=base;++x.revision;changed(x);
    x=base;++x.luid_low;changed(x);x=base;++x.luid_high;changed(x);x=base;++x.node_count;changed(x);x=base;x.software=true;changed(x);
    x=base;++x.driver;changed(x);x=base;++x.d3d12;changed(x);x=base;++x.dxgi;changed(x);x=base;++x.compiler;changed(x);
    x=base;x.core_loaded=true;x.core=1;changed(x);++x.core;changed(x);x=base;x.producer.push_back(0);changed(x);
    auto invalid=[&](ScopeIdentity v){require(compatibility_scope(v).empty(),"incomplete identity accepted");};
    invalid({});x=base;x.vendor=0;invalid(x);x=base;x.device=0;invalid(x);x=base;x.luid_low=x.luid_high=0;invalid(x);
    x=base;x.node_count=0;invalid(x);x.node_count=33;invalid(x);x=base;x.driver=0;invalid(x);x=base;x.d3d12=0;invalid(x);
    x=base;x.dxgi=0;invalid(x);x=base;x.compiler=0;invalid(x);x=base;x.core_loaded=true;invalid(x);x=base;x.core=1;invalid(x);
    x=base;x.producer.clear();invalid(x);x=base;x.producer.resize(1025);invalid(x);x.producer.resize(1024);require(!compatibility_scope(x).empty(),"producer cap");
    auto dir=std::filesystem::temp_directory_path()/("nativex-scope-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));require(std::filesystem::create_directory(dir),"temp directory");
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}} cleanup{dir};
    auto file=dir/"cache.bin";Cache saved(identity);require(saved.put({1},{2,3})==Status::Ok&&saved.save(file)==Status::Ok,"save");
    unsigned mismatches=0;for(const auto& scope:distinct){Cache c(scope);require(c.put({9},{8})==Status::Ok,"initial");const auto status=c.load(file);if(scope==identity)require(status==Status::Ok&&c.find({1})&&!c.find({9}),"matching load");else{require(status==Status::ScopeMismatch&&c.find({9})&&!c.find({1}),"incompatible cache admitted");++mismatches;}}
    std::cout<<"{\"scope_variants\":"<<distinct.size()<<",\"incompatible_rejected\":"<<mismatches<<",\"matching_loaded\":1,\"errors\":0}\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
