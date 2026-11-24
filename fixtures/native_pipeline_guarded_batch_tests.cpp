#include "native_pipeline_failure.hpp"
#include <atomic>
#include <iostream>
#include <random>
#include <set>
using namespace scarlett::pipeline;
void check(bool b){if(!b)throw std::runtime_error("guarded batch invariant");}
Bytes key(unsigned n){return {static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(n>>8)};}
ClassifiedAttempt unsupported(){return {{AttemptStatus::Failed,{}},FailureClass::DeterministicUnsupported};}
ClassifiedAttempt success(){return {{AttemptStatus::Created,{42}},FailureClass::None};}
int main(){
    GuardedPipelineCache c({1},16,32);std::atomic<unsigned> calls{0};
    check(c.create(key(1),[](const Bytes*){return unsupported();}).remembered);
    std::vector<BatchRequest> requests{{key(1),999,Priority::FrameCritical},{key(2),8},{key(2),8,Priority::FrameCritical},{key(3),8},{key(4),8},{key(1),8},{key(5),0},{key(6),999}};
    auto callback=[&](const Bytes& k,const Bytes*)->ClassifiedAttempt{++calls;if(k==key(2))return unsupported();if(k==key(3))return {};if(k==key(4))return success();throw std::runtime_error("unexpected key");};
    auto a=prepare_guarded_batch(c,requests,callback,{2,16});
    check(calls==3&&a.unique_jobs==5&&a.suppressed_jobs==1&&a.waves==2&&a.peak_wave_jobs==2&&a.peak_estimated_bytes==16);
    check(a.items[0].status==BatchStatus::Suppressed&&a.items[5].coalesced&&a.items[5].status==BatchStatus::Suppressed);
    check(a.items[1].failure_remembered&&a.items[2].failure_remembered&&a.items[2].coalesced);
    check(a.items[3].build.path==BuildPath::Failed&&!a.items[3].failure_remembered&&a.items[4].build.path==BuildPath::Cold);
    check(a.items[6].status==BatchStatus::Invalid&&a.items[7].status==BatchStatus::BudgetExceeded);
    check(c.create(key(2),[](const Bytes*){return success();}).suppressed);
    a=prepare_guarded_batch(c,requests,callback,{2,16});check(calls==5&&a.suppressed_jobs==2&&a.waves==1&&a.items[4].build.path==BuildPath::Hit);
    check(c.forget(key(2)));check(c.create(key(2),[](const Bytes*){return success();}).build.path==BuildPath::Cold);
    a=prepare_guarded_batch(c,{{key(2),8}},[](const Bytes&,const Bytes* cached){return cached?ClassifiedAttempt{{AttemptStatus::CacheRejected,{}},FailureClass::None}:unsupported();});
    check(a.items[0].failure_remembered&&a.items[0].build.attempts==2&&a.items[0].build.cache_rejected);
    a=prepare_guarded_batch(c,{{key(7),8}},[](const Bytes&,const Bytes*)->ClassifiedAttempt{throw std::runtime_error("transient");});check(a.items[0].status==BatchStatus::CallbackException&&!a.items[0].failure_remembered);
    check(!c.create(key(7),[](const Bytes*){return success();}).suppressed);
    GuardedPipelineCache stress({2},128,256);std::set<unsigned> known;std::mt19937 rng(2026);std::size_t suppressed=0,executed=0;
    for(unsigned round=0;round<500;++round){
        if(round%25==0){stress.clear_failures();known.clear();}
        std::vector<BatchRequest> batch;std::set<unsigned> unique,expected_run;
        for(unsigned i=0;i<64;++i){unsigned n=rng()%128;batch.push_back({key(n),8,static_cast<Priority>(rng()%3)});unique.insert(n);if(!known.count(n))expected_run.insert(n);}
        std::atomic<unsigned> invoked{0};auto res=prepare_guarded_batch(stress,batch,[&](const Bytes& k,const Bytes*){++invoked;return k[0]%3==0?unsupported():ClassifiedAttempt{};},{4,32});
        check(invoked==expected_run.size()&&res.unique_jobs==unique.size()&&res.suppressed_jobs==unique.size()-expected_run.size()&&res.peak_wave_jobs<=4&&res.peak_estimated_bytes<=32);
        for(std::size_t i=0;i<batch.size();++i){unsigned n=batch[i].key[0];check((res.items[i].status==BatchStatus::Suppressed)==(known.count(n)!=0));}
        for(auto n:expected_run)if(n%3==0)known.insert(n);
        check(stress.failure_count()==known.size()&&stress.failure_key_bytes()==known.size()*2);suppressed+=res.suppressed_jobs;executed+=invoked;
    }
    std::cout<<"batches=500 requests=32000 suppressed_unique="<<suppressed<<" executed_unique="<<executed<<" errors=0\n";
}
