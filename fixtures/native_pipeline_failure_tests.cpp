#include "native_pipeline_failure.hpp"
#include <iostream>
#include <random>
#include <set>
using namespace scarlett::pipeline;
void check(bool b){if(!b)throw std::runtime_error("failure-cache invariant");}
ClassifiedAttempt unsupported(){return {{AttemptStatus::Failed,{}},FailureClass::DeterministicUnsupported};}
ClassifiedAttempt created(){return {{AttemptStatus::Created,{42}},FailureClass::None};}
Bytes key(unsigned n){return {static_cast<std::uint8_t>(n),static_cast<std::uint8_t>(n>>8)};}
int main(){
    GuardedPipelineCache c({1},2,4);unsigned calls=0;
    auto fail=[&](const Bytes*){++calls;return unsupported();};
    auto a=c.create(key(1),fail);check(a.remembered&&a.build.attempts==1);
    for(unsigned i=0;i<10000;++i){a=c.create(key(1),fail);check(a.suppressed&&a.build.attempts==0);}
    check(calls==1);check(c.create(key(2),fail).remembered);
    check(!c.create(key(3),fail).remembered&&!c.create(key(3),fail).suppressed);
    check(c.failure_count()==2&&c.failure_key_bytes()==4);
    check(c.forget(key(1))&&!c.forget(key(1)));check(c.create(key(3),fail).remembered);
    c.clear_failures();check(c.failure_count()==0&&c.failure_key_bytes()==0);
    for(unsigned i=0;i<10;++i)check(!c.create(key(4),[](const Bytes*){return ClassifiedAttempt{};}).remembered);
    check(c.create(key(4),[](const Bytes*){return created();}).build.path==BuildPath::Cold);
    check(!c.create(key(4),fail).remembered); // cached-only failure not permanent
    a=c.create(key(4),[](const Bytes* b){return b?ClassifiedAttempt{{AttemptStatus::CacheRejected,{}},FailureClass::None}:unsupported();});
    check(a.remembered&&a.build.attempts==2&&a.build.cache_rejected);
    check(c.create(key(4),fail).suppressed);c.forget(key(4));
    check(c.create(key(4),[](const Bytes*){return created();}).build.path==BuildPath::Cold);
    bool threw=false;try{c.create(key(9),[](const Bytes*)->ClassifiedAttempt{throw std::runtime_error("transient");});}catch(const std::runtime_error&){threw=true;}
    check(threw&&!c.create(key(9),[](const Bytes*){return created();}).suppressed);
    check(c.create({},fail).build.path==BuildPath::Invalid);
    GuardedPipelineCache disabled({1},0,0);check(!disabled.create(key(1),fail).remembered);
    GuardedPipelineCache fresh({2});check(!fresh.create(key(4),fail).suppressed);
    // Differential model: independent set with fixed admission limits.
    GuardedPipelineCache stress({3},64,128);std::set<unsigned> expected;std::mt19937 rng(2026);unsigned suppressed=0,invoked=0;
    for(unsigned i=0;i<50000;++i){unsigned n=rng()%128,op=rng()%8;
        if(op==0){check(stress.forget(key(n))==(expected.erase(n)!=0));}
        else if(op==1){stress.clear_failures();expected.clear();}
        else {const bool was=expected.count(n)!=0;const bool permanent=op!=2;
            auto r=stress.create(key(n),[&](const Bytes*){++invoked;return permanent?unsupported():ClassifiedAttempt{};});
            check(r.suppressed==was);if(was)++suppressed;
            else if(permanent&&expected.size()<64){check(r.remembered);expected.insert(n);}else check(!r.remembered);
        }
        check(stress.failure_count()==expected.size()&&stress.failure_key_bytes()==expected.size()*2);
    }
    std::cout<<"repeat_requests=10000 repeated_callbacks=0 model_operations=50000 suppressed="<<suppressed<<" invoked="<<invoked<<" errors=0\n";
}
