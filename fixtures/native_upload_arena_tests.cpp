#include "native_upload_arena.hpp"
#include "native_texture_upload.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>
using namespace scarlett::upload;
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void controls(){
    UploadArena a(2048,4);
    auto x=a.reserve(256),y=a.reserve(512),z=a.reserve(512);
    require(x&&y&&z&&x->offset==0&&y->offset==512&&z->offset==1024,"alignment");
    auto gap=a.reserve(128,128);require(gap&&gap->offset==256,"gap reuse");
    require(!a.reserve(1,1),"metadata limit");
    require(a.seal(*x,1)&&a.seal(*y,2)&&a.seal(*z,2),"seal");
    require(!a.cancel(*y)&&!a.seal(*gap,1),"unsafe retirement or fence regression");
    require(!a.collect(UINT64_MAX)&&a.stats().in_flight==1280,"device loss retired data");
    require(a.collect(1)&&a.stats().used==1152,"first fence");
    auto reuse=a.reserve(256);require(reuse&&reuse->offset==0&&reuse->id!=x->id,"reuse");
    require(!a.cancel(*x)&&!a.seal(*x,3),"stale token");
    require(!a.collect(0)&&!a.seal(*reuse,1),"old timeline");
    require(a.collect(2)&&a.stats().reserved==384&&a.stats().in_flight==0,"reserved data freed");
    require(a.cancel(*gap)&&a.cancel(*reuse)&&a.stats().used==0,"cancel");
    require(!a.reserve(0)&&!a.reserve(1,3)&&!a.reserve(2049)&&!a.reserve(1,131072),"invalid range");
    bool bad=false;try{UploadArena invalid(0);}catch(const std::invalid_argument&){bad=true;}require(bad,"invalid config");
}
void staging(){
    std::vector<std::uint8_t> red(4*4*4,0),green(red.size(),0);
    for(std::size_t i=0;i<red.size();i+=4){red[i]=255;red[i+3]=255;green[i+1]=255;green[i+3]=255;}
    auto a=stage({Format::Rgba8,4,4,1,1},{{red.data(),red.size(),16}});
    auto b=stage({Format::Rgba8,4,4,1,1},{{green.data(),green.size(),16}});
    require(a.plan.status==Status::Ready&&b.plan.status==Status::Ready,"stage failed");
    UploadArena arena(a.bytes.size()*2);std::vector<std::uint8_t> backing(a.bytes.size()*2);
    auto x=arena.reserve(a.bytes.size()),y=arena.reserve(b.bytes.size());require(x&&y,"stage reserve");
    std::copy(a.bytes.begin(),a.bytes.end(),backing.begin()+static_cast<std::ptrdiff_t>(x->offset));
    std::copy(b.bytes.begin(),b.bytes.end(),backing.begin()+static_cast<std::ptrdiff_t>(y->offset));
    require(arena.seal(*x,1)&&arena.seal(*y,2)&&!arena.reserve(1),"pending data overwritten");
    require(arena.collect(1),"collect first");auto z=arena.reserve(a.bytes.size());require(z&&z->offset==x->offset,"first slice not reused");
    std::fill_n(backing.begin()+static_cast<std::ptrdiff_t>(z->offset),z->bytes,0x7f);
    require(std::equal(b.bytes.begin(),b.bytes.end(),backing.begin()+static_cast<std::ptrdiff_t>(y->offset)),"in-flight page changed");
    require(arena.cancel(*z)&&arena.collect(2)&&arena.stats().used==0,"staging cleanup");
}
void randomized(){
    constexpr std::size_t cap=4096;UploadArena arena(cap,64);
    struct Record{Slice slice;std::uint64_t fence=0;};std::vector<Record> live;
    std::mt19937 rng(20261124);std::uint64_t fence=0,completed=0;unsigned accepted=0,denied=0;
    for(unsigned step=0;step<20000;++step){
        auto op=rng()%4;
        if(op==0||live.empty()){
            auto bytes=static_cast<std::size_t>(rng()%700+1),alignment=std::size_t{1}<<(rng()%10);
            // Independent byte occupancy and earliest legal aligned address.
            std::vector<bool> occupied(cap,false);
            for(const auto& r:live)for(auto i=r.slice.offset;i<r.slice.offset+r.slice.bytes;++i)occupied[i]=true;
            std::optional<std::size_t> expected;
            if(live.size()<64)for(std::size_t i=0;i+bytes<=cap;i+=alignment){if(std::none_of(occupied.begin()+static_cast<std::ptrdiff_t>(i),occupied.begin()+static_cast<std::ptrdiff_t>(i+bytes),[](bool b){return b;})){expected=i;break;}}
            auto s=arena.reserve(bytes,alignment);require(bool(s)==bool(expected),"admission disagrees with bitmap");
            if(s){require(s->offset==*expected&&s->bytes==bytes,"first fit mismatch");live.push_back({*s,0});++accepted;}else ++denied;
        }else if(op==1){auto& r=live[rng()%live.size()];if(!r.fence){fence=std::max(fence,completed)+1;require(arena.seal(r.slice,fence),"seal failed");r.fence=fence;}else require(!arena.cancel(r.slice),"cancel in-flight");}
        else if(op==2){auto i=static_cast<std::size_t>(rng()%live.size());if(!live[i].fence){require(arena.cancel(live[i].slice),"cancel failed");live.erase(live.begin()+static_cast<std::ptrdiff_t>(i));}}
        else {if(completed<fence)++completed;require(arena.collect(completed),"collect failed");live.erase(std::remove_if(live.begin(),live.end(),[&](const Record& r){return r.fence&&r.fence<=completed;}),live.end());}
        std::size_t reserved=0,flight=0;for(const auto& r:live)(r.fence?flight:reserved)+=r.slice.bytes;
        auto s=arena.stats();require(s.used==reserved+flight&&s.reserved==reserved&&s.in_flight==flight&&s.allocations==live.size()&&s.peak<=cap,"accounting mismatch");
    }
    for(const auto& r:live)if(!r.fence)require(arena.cancel(r.slice),"final cancel");
    require(arena.collect(fence)&&arena.stats().used==0&&arena.stats().allocations==0,"final cleanup");
    std::cout<<"{\"operations\":20000,\"accepted\":"<<accepted<<",\"denied\":"<<denied<<",\"peak_bytes\":"<<arena.stats().peak<<",\"final_bytes\":0}\n";
}
}
int main(){try{controls();staging();randomized();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
