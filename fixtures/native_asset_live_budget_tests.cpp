// Original test payloads only. Live ownership accounting, no external assets.
#include "native_asset_singleflight.hpp"
#include <iostream>
#include <stdexcept>
using namespace scarlett::stream;
namespace {
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
Result payload(std::size_t n,std::uint8_t v=42){Result r{Status::Ok,std::vector<std::uint8_t>(n,v),0};r.checksum=fnv1a64(r.bytes);return r;}
void controls(){
    bool rejected=false;try{AssetCache invalid(8,2,7);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"live limit smaller than resident limit");
    AssetCache cache(8,2,12);
    CacheKey a{"a",1,0,8},b{"b",1,0,4},c{"c",1,0,8};
    auto held=cache.publish(a,payload(8)).result;
    require(held&&cache.stats().live_bytes==8,"initial live bytes");
    auto alias=cache.find(a);
    require(alias==held&&cache.stats().live_bytes==8,"duplicate owners charged twice");
    require(cache.publish(b,payload(4)).status==CacheInsert::Inserted,"second result rejected");
    require(cache.stats().resident_bytes==4&&cache.stats().live_bytes==12,"evicted pinned buffer lost credit");
    auto deny=cache.publish(c,payload(8));
    require(deny.status==CacheInsert::LiveBudgetExceeded&&!deny.result,"pinned cap bypass");
    require(cache.stats().live_bytes==8&&cache.stats().live_rejections==1,"pressure reclaim accounting");
    held.reset();require(cache.stats().live_bytes==8,"released non-final owner");
    alias.reset();require(cache.stats().live_bytes==0,"last owner did not return credit");
    auto fresh=cache.publish(c,payload(8)).result;
    require(bool(fresh),"credit not reusable");
    cache.clear();require(cache.stats().resident_bytes==0&&cache.stats().live_bytes==8,"clear uncharged external owner");
    fresh.reset();require(cache.stats().live_bytes==0,"clear final release");
    auto uncached=cache.publish({"large",1,0,12},payload(12));
    require(uncached.status==CacheInsert::TooLarge&&uncached.result&&cache.stats().live_bytes==12,"uncached result not charged");
    require(cache.publish(a,payload(8)).status==CacheInsert::LiveBudgetExceeded,"uncached credit bypass");
    uncached.result.reset();require(cache.stats().live_bytes==0,"uncached result not released");
    require(cache.publish({"huge",1,0,13},payload(13)).status==CacheInsert::LiveBudgetExceeded,"single oversize live request");
    require(cache.stats().peak_live_bytes==12,"peak accounting");
    std::shared_ptr<const Result> survivor;
    {AssetCache short_lived(4,1,4);survivor=short_lived.publish({"s",1,0,4},payload(4)).result;}
    require(survivor&&survivor->bytes[0]==42,"cache lifetime destroyed consumer");survivor.reset();
}
void reader_integration(){
    unsigned calls=0;
    CoalescedReader reader([&](Request r){++calls;return payload(r.length);},8,2,4,8);
    auto first=reader.read({"a",1,0,8},"fixture");
    auto denied=reader.read({"b",1,0,8},"fixture");
    require(first->status==Status::Ok&&denied->status==Status::BudgetExceeded,"adapter backpressure status");
    require(reader.stats().active==0&&reader.cache_stats().live_bytes==8,"flight leak or incorrect credit");
    first.reset();
    auto retry=reader.read({"b",1,0,8},"fixture");
    require(retry->status==Status::Ok&&calls==2,"retry after release; denied request must not read");
}
void concurrent(){
    AssetCache cache(512,8,1024);
    std::atomic<unsigned> errors{0},ok{0},denied{0};
    std::vector<std::thread> threads;
    for(unsigned t=0;t<8;++t)threads.emplace_back([&,t]{
        std::vector<std::shared_ptr<const Result>> held;
        for(unsigned i=0;i<512;++i){
            auto v=static_cast<std::uint8_t>((i+t)%16);
            CacheKey key{"resource",1,static_cast<std::uint64_t>(v)*128,128};
            auto p=cache.publish(key,payload(128,v));
            if(p.status==CacheInsert::LiveBudgetExceeded){++denied;}
            else if(p.result&&p.result->bytes.size()==128&&p.result->bytes[0]==v){++ok;held.push_back(p.result);}
            else ++errors;
            if(held.size()>3)held.erase(held.begin());
            if(i%17==0)held.clear();
            if(i%79==0)cache.invalidate("resource");
            auto s=cache.stats();if(s.live_bytes>s.live_limit||s.peak_live_bytes>s.live_limit||s.resident_bytes>s.live_bytes||s.resident_bytes>s.byte_limit)++errors;
        }
    });
    for(auto& t:threads)t.join();
    cache.clear();
    auto s=cache.stats();require(errors==0&&ok+denied==4096&&ok>0&&s.live_bytes==0,"concurrent live accounting");
    std::cout<<"{\"attempts\":4096,\"accepted\":"<<ok<<",\"denied\":"<<denied<<",\"peak_live_bytes\":"<<s.peak_live_bytes<<",\"live_limit\":"<<s.live_limit<<",\"final_live_bytes\":"<<s.live_bytes<<",\"errors\":"<<errors<<"}\n";
}
}
int main(){try{controls();reader_integration();concurrent();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
