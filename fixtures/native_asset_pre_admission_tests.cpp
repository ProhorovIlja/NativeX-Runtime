// Independent original test data; no proprietary files or platform dependencies.
#include "native_asset_singleflight.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <utility>
using namespace scarlett::stream;
namespace {
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
Result payload(std::size_t n,std::uint8_t byte=7){Result r{Status::Ok,std::vector<std::uint8_t>(n,byte),0};r.checksum=fnv1a64(r.bytes);return r;}
void accounting(const AssetCache& cache,std::size_t live,std::size_t reserved){
    const auto s=cache.stats();
    require(s.live_bytes==live&&s.reserved_bytes==reserved&&s.committed_bytes==live+reserved,"reservation accounting");
    require(!s.live_limit||s.peak_committed_bytes<=s.live_limit,"commitment cap exceeded");
}
void controls(){
    AssetCache c(8,2,12),other(8,2,12);
    CacheKey a{"a",1,0,8},b{"b",1,0,4};
    auto t=c.reserve(8);require(bool(t),"initial reservation");accounting(c,0,8);
    require(!c.reserve(5),"overcommit admitted");
    auto wrong=other.publish_reserved(a,payload(8),std::move(t));
    require(wrong.status==CacheInsert::Invalid&&!t,"foreign ticket accepted");accounting(c,0,0);accounting(other,0,0);
    t=c.reserve(8);auto moved=std::move(t);require(!t&&bool(moved),"move ownership");
    auto bad=c.publish_reserved(b,payload(4),std::move(moved));
    require(bad.status==CacheInsert::Invalid,"length mismatch accepted");accounting(c,0,0);
    auto z=c.reserve(0);require(bool(z),"valid zero-byte ticket");
    auto zero=c.publish_reserved({"zero",1,0,0},payload(0),std::move(z));
    require(zero.status==CacheInsert::Inserted&&zero.result&&!z,"zero publication");
    t=c.reserve(8);auto p=c.publish_reserved(a,payload(8),std::move(t));
    require(p.status==CacheInsert::Inserted&&p.result,"reservation not transferred");accounting(c,8,0);
    // A duplicate needs temporary credit, but publication must not charge twice.
    auto four=c.reserve(4);require(bool(four),"second reservation");accounting(c,8,4);
    require(!c.reserve(1),"pinned plus reserved cap bypass");
    four=AssetCache::Reservation{};accounting(c,8,0);
    require(!c.reserve(8),"pinned buffer ignored");
    p.result.reset(); // cache-only entry may be reclaimed to admit a new read
    auto another=c.reserve(8);require(bool(another),"reclaim failed");accounting(c,0,8);
    auto invalid=payload(8);invalid.checksum=0;
    require(c.publish_reserved(a,std::move(invalid),std::move(another)).status==CacheInsert::Invalid,"invalid publication");accounting(c,0,0);
    auto small=c.publish(b,payload(4));require(bool(small.result),"direct publish");
    auto duplicate=c.reserve(4);auto same=c.publish_reserved(b,payload(4),std::move(duplicate));
    require(same.status==CacheInsert::AlreadyPresent&&same.result==small.result,"duplicate ticket transfer");accounting(c,4,0);
    auto conflict=c.reserve(4);require(c.publish_reserved(b,payload(4,8),std::move(conflict)).status==CacheInsert::Conflict,"conflicting result accepted");accounting(c,4,0);
    auto uncached_ticket=c.reserve(8); // live 4 + reserved 8
    require(bool(uncached_ticket),"reservation exact cap");
    auto no_cache=c.publish_reserved({"big",1,0,8},payload(8),std::move(uncached_ticket));
    require(bool(no_cache.result),"exact cap transfer");accounting(c,12,0);
    c.clear();accounting(c,12,0);small.result.reset();same.result.reset();no_cache.result.reset();accounting(c,0,0);
    auto long_ticket=c.reserve(12);
    auto oversized=c.publish_reserved({"large",1,0,12},payload(12),std::move(long_ticket));
    require(oversized.status==CacheInsert::TooLarge&&oversized.result,"uncached credit transfer");accounting(c,12,0);
    oversized.result.reset();accounting(c,0,0);
    AssetCache::Reservation survivor;
    {AssetCache short_lived(4,1,4);survivor=short_lived.reserve(4);}
    require(bool(survivor),"ticket lifetime");survivor=AssetCache::Reservation{};
}
void failure_paths(){
    unsigned calls=0;
    CoalescedReader r([&](Request req)->Result{++calls;if(calls==1)return {Status::FileError,{},0};if(calls==2)throw std::runtime_error("backend failure");if(calls==3){auto p=payload(req.length);p.checksum=0;return p;}return payload(req.length);},8,1,2,8);
    CacheKey key{"asset",1,0,8};
    require(r.read(key,"fixture")->status==Status::FileError,"read failure");
    require(r.cache_stats().committed_bytes==0,"failed read retained reservation");
    for(int i=0;i<2;++i){bool thrown=false;try{r.read(key,"fixture");}catch(const std::runtime_error&){thrown=true;}require(thrown&&r.cache_stats().committed_bytes==0&&r.stats().active==0,"exception or invalid result retained credit");}
    auto good=r.read(key,"fixture");require(good->status==Status::Ok&&calls==4,"retry after failure");
    auto denied=r.read({"other",1,0,8},"fixture");require(denied->status==Status::BudgetExceeded&&calls==4,"denied request reached backend");
    good.reset();require(r.read({"other",1,0,8},"fixture")->status==Status::Ok&&calls==5,"reuse after release");
}
void in_flight(){
    std::mutex mutex;std::condition_variable cv;bool release=false;unsigned entered=0;
    CoalescedReader r([&](Request req){std::unique_lock<std::mutex> lock(mutex);++entered;cv.notify_all();cv.wait(lock,[&]{return release;});return payload(req.length);},8,2,8,8);
    auto a=std::async(std::launch::async,[&]{return r.read({"one",1,0,8},"fixture");});
    bool started;
    {std::unique_lock<std::mutex> lock(mutex);started=cv.wait_for(lock,std::chrono::seconds(3),[&]{return entered==1;});}
    auto b=std::async(std::launch::async,[&]{return r.read({"two",1,0,8},"fixture");});
    const bool rejected=b.wait_for(std::chrono::seconds(3))==std::future_status::ready;
    auto snapshot=r.cache_stats();
    {std::lock_guard<std::mutex> lock(mutex);release=true;}cv.notify_all();
    auto first=a.get(),second=b.get();
    require(started&&rejected&&second->status==Status::BudgetExceeded,"in-flight admission did not reject promptly");
    require(snapshot.live_bytes==0&&snapshot.reserved_bytes==8&&snapshot.committed_bytes==8,"pending credit not visible");
    require(first->status==Status::Ok&&entered==1&&r.stats().leader_reads==1&&r.stats().rejected==1,"unexpected duplicate I/O");
    require(r.cache_stats().reserved_bytes==0&&r.cache_stats().live_bytes==8,"transfer accounting");
}
void concurrent(){
    AssetCache cache(512,8,1024);
    std::atomic<unsigned> errors{0},accepted{0},denied{0};
    std::vector<std::thread> threads;
    for(unsigned t=0;t<8;++t)threads.emplace_back([&,t]{
        std::vector<std::shared_ptr<const Result>> held;
        for(unsigned i=0;i<512;++i){
            auto value=static_cast<std::uint8_t>((t+i)%16);
            auto ticket=cache.reserve(128);
            if(ticket){auto r=cache.publish_reserved({"source",1,static_cast<std::uint64_t>(value)*128,128},payload(128,value),std::move(ticket));
                if(!r.result||r.result->bytes.size()!=128||r.result->bytes.front()!=value)++errors;
                else{++accepted;held.push_back(r.result);}}
            else ++denied;
            if(held.size()>3)held.erase(held.begin());
            if(i%17==0)held.clear();
            if(i%79==0)cache.invalidate("source");
            const auto s=cache.stats();
            if(s.committed_bytes!=s.live_bytes+s.reserved_bytes||s.committed_bytes>s.live_limit||s.peak_committed_bytes>s.live_limit||s.resident_bytes>s.live_bytes||s.resident_bytes>s.byte_limit)++errors;
        }
    });
    for(auto& t:threads)t.join();
    cache.clear();
    const auto s=cache.stats();require(errors==0&&accepted+denied==4096&&accepted>0&&s.committed_bytes==0,"concurrent reservation invariant");
    std::cout<<"{\"attempts\":4096,\"accepted\":"<<accepted<<",\"denied\":"<<denied<<",\"peak_committed_bytes\":"<<s.peak_committed_bytes<<",\"limit\":"<<s.live_limit<<",\"final_committed_bytes\":"<<s.committed_bytes<<",\"errors\":"<<errors<<"}\n";
}
}
int main(){try{controls();failure_paths();in_flight();concurrent();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
