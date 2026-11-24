#include "native_pipeline_batch.hpp"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace scarlett::pipeline;
namespace {
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
Bytes key(unsigned i){return {static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(i>>8)};}
void controls(){
    Cache cache({1},4096,64);for(unsigned i:{1u,2u,3u,4u})check(cache.put(key(i),{9})==Status::Ok,"seed");
    std::vector<BatchRequest> req{{key(0),4},{key(1),4},{key(2),4},{key(3),4},{key(4),4},{key(5),4},{key(2),6},{key(6),100},{}, {key(7),0},{key(8),1},{key(9),4}};
    std::atomic<unsigned> calls{0};
    auto r=prepare_batch(cache,req,[&](const Bytes& k,const Bytes* b)->Attempt{++calls;
        switch(k[0]){case 0:return {AttemptStatus::Created,{1}};case 1:check(b&&*b==Bytes{9},"hit blob");return {AttemptStatus::Created,{}};
        case 2:return b?Attempt{AttemptStatus::CacheRejected,{}}:Attempt{AttemptStatus::Created,{2}};
        case 3:return {AttemptStatus::Failed,{}};case 4:throw std::runtime_error("backend exception");case 5:return {AttemptStatus::Created,{}};
        case 8:return {AttemptStatus::Created,{1,2}};case 9:return {AttemptStatus::Failed,{}};default:throw std::runtime_error("denied callback");}
    },{3,12});
    check(calls==9&&r.unique_jobs==9&&r.items.size()==req.size(),"dedup counts");
    check(r.items[0].build.path==BuildPath::Cold&&r.items[0].build.write==WriteStatus::Stored,"cold");
    check(r.items[1].build.path==BuildPath::Hit,"hit");
    check(r.items[2].build.path==BuildPath::Rebuilt&&r.items[2].build.attempts==2&&r.items[6].coalesced&&r.items[6].build.path==BuildPath::Rebuilt&&*cache.find(key(2))==Bytes{2},"fallback duplicate");
    check(r.items[3].build.path==BuildPath::Failed&&*cache.find(key(3))==Bytes{9},"unrelated failure");
    check(r.items[4].status==BatchStatus::CallbackException&&*cache.find(key(4))==Bytes{9},"exception");
    check(r.items[5].build.write==WriteStatus::Unavailable&&r.items[7].status==BatchStatus::BudgetExceeded,"budget/no blob");
    check(r.items[8].status==BatchStatus::Invalid&&r.items[9].status==BatchStatus::Invalid,"invalid request");
    check(r.items[10].build.path==BuildPath::Cold&&r.items[10].build.write==WriteStatus::Rejected&&!cache.find(key(8)),"oversized output");
    check(r.peak_wave_jobs<=3&&r.peak_estimated_bytes<=12,"admission bound");
    auto d=prepare_batch(cache,{{key(10),1},{key(10),13}},[](const Bytes&,const Bytes*){throw std::runtime_error("denied duplicate called");return Attempt{};},{2,12});
    check(d.items[0].status==BatchStatus::BudgetExceeded&&d.items[1].coalesced&&d.waves==0,"max duplicate estimate");
    bool bad=false;try{prepare_batch(cache,{},[](const Bytes&,const Bytes*){return Attempt{};},{0,1});}catch(const std::invalid_argument&){bad=true;}check(bad,"bad config");
    check(prepare_batch(cache,{},[](const Bytes&,const Bytes*){return Attempt{};}).items.empty(),"empty");
    Cache tiny({1},25);auto full=prepare_batch(tiny,{{key(0),4}},[](const Bytes&,const Bytes*){return Attempt{AttemptStatus::Created,{1}};});check(full.items[0].build.path==BuildPath::Cold&&full.items[0].build.write==WriteStatus::Rejected,"cache full not build failure");
    auto retry=prepare_batch(cache,{{key(1),4}},[](const Bytes&,const Bytes* b)->Attempt{if(b)return {AttemptStatus::CacheRejected,{}};throw std::runtime_error("retry");});check(retry.items[0].status==BatchStatus::CallbackException&&!cache.find(key(1))&&retry.items[0].build.attempts==2,"retry exception stale blob");
}
void concurrency(){
    Cache cache({1});std::mutex m;std::condition_variable cv;unsigned entered=0,active=0,peak=0;
    // Force genuine overlap, not just a claim based on worker count.
    auto r=prepare_batch(cache,{{key(0),8},{key(1),8},{key(2),8}},[&](const Bytes&,const Bytes*){
        std::unique_lock<std::mutex> lock(m);++entered;++active;peak=std::max(peak,active);cv.notify_all();cv.wait(lock,[&]{return entered==3;});--active;return Attempt{AttemptStatus::Created,{1}};},{3,24});
    check(peak==3&&r.peak_wave_jobs==3&&r.peak_estimated_bytes==24&&cache.size()==3,"real overlap");
    std::cout<<"actual_overlap="<<peak<<'\n';
}
void stress(){
    Cache cache({1},1024*1024,4096);std::vector<BatchRequest> requests;
    for(unsigned i=0;i<4096;++i)requests.push_back({key(i%512),1+(i%512)%11});
    std::atomic<unsigned> calls{0},active{0},peak{0};std::atomic<std::size_t> bytes{0},peak_bytes{0};
    auto op=[&](const Bytes& k,const Bytes* b){++calls;const auto n=static_cast<unsigned>(k[0])+(static_cast<unsigned>(k[1])<<8);auto a=++active;auto old=peak.load();while(old<a&&!peak.compare_exchange_weak(old,a)){};auto v=bytes.fetch_add(1+n%11)+1+n%11;auto prev=peak_bytes.load();while(prev<v&&!peak_bytes.compare_exchange_weak(prev,v)){};std::this_thread::yield();bytes.fetch_sub(1+n%11);--active;return b?Attempt{AttemptStatus::Created,{}}:Attempt{AttemptStatus::Created,{k[0]}};};
    auto cold=prepare_batch(cache,requests,op,{4,24});auto warm=prepare_batch(cache,requests,op,{4,24});
    check(calls==1024&&cache.size()==512&&active==0&&bytes==0&&peak<=4&&peak_bytes<=24,"stress counters");
    for(unsigned i=0;i<requests.size();++i){check(cold.items[i].status==BatchStatus::Completed&&cold.items[i].build.path==BuildPath::Cold&&warm.items[i].build.path==BuildPath::Hit,"stress routes");check(cold.items[i].coalesced==(i>=512),"stress coalescing");}
    check(cold.peak_estimated_bytes<=24&&warm.peak_estimated_bytes<=24,"wave limit");
    std::cout<<"{\"requests\":8192,\"unique_callbacks\":"<<calls<<",\"coalesced\":7168,\"peak_active\":"<<peak<<",\"peak_active_estimate\":"<<peak_bytes<<",\"waves\":"<<cold.waves+warm.waves<<",\"errors\":0}\n";
}
}
int main(){try{controls();concurrency();stress();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
