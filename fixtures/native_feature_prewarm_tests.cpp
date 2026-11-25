#include "native_feature_prewarm.hpp"
#include <iostream>
#include <random>
#include <atomic>
using namespace scarlett;
using namespace scarlett::features;
void check(bool b){if(!b)throw std::runtime_error("feature/prewarm mismatch");}
void capability_tests(){
    graphics::Settings r;graphics::Capabilities gc{true,true,false,{}};const auto g=graphics::resolve(r,gc);
    unsigned cases=0;
    for(unsigned m=0;m<1024;++m)for(unsigned u=0;u<4;++u){Caps c;c.vendor=(m&1)?Vendor::Nvidia:Vendor::Other;c.available_inputs=(m&2)?Temporal:Color;
        for(auto& v:c.sr)v={bool(m&4),bool(m&8),Temporal};
        c.reflex_ready=bool(m&16);c.reflex_markers=bool(m&32);c.ds_gpu_verified=bool(m&64);c.ds_cpu_verified=bool(m&128);c.win32_verified=bool(m&256);
        Settings s{static_cast<Upscaler>(u),bool(m&512),true,true,true};auto p=resolve(r,g,s,c);
        bool sr=u==0||((m&14)==14&&(u!=1||(m&1)));bool storage=(m&(64|128|256))!=0;
        check(p.ready==(storage&&(sr||s.allow_native_fallback)));
        if(p.ready){check(p.sr==(sr?s.requested:Upscaler::Native));check(p.sr_fallback==!sr);check(bool(p.enabled&Reflex)==((m&49)==49));check(!(p.enabled&Ray));
            check(p.storage==((m&64)?Storage::DirectStorageGPU:((m&128)?Storage::DirectStorageCPU:Storage::Win32)));check((p.enabled&7)==(u&&sr?(1u<<(u-1)):0));}
        ++cases;
    }
    Caps c;c.win32_verified=true;c.backend=graphics::Backend::Vulkan;check(!resolve(r,g,Settings{},c).ready);
    c.backend=graphics::Backend::D3D12;c.ds_cpu_verified=true;c.ds_gpu_verified=true;
    Settings s;s.gpu_decode=false;check(resolve(r,g,s,c).storage==Storage::DirectStorageCPU);
    s.directstorage=false;check(resolve(r,g,s,c).storage==Storage::Win32);
    c.win32_verified=false;check(!resolve(r,g,s,c).ready);
    c.win32_verified=true;s.requested=static_cast<Upscaler>(99);check(!resolve(r,g,s,c).ready);
    std::cout<<"capability_cases="<<cases<<'\n';
}
void integration(){using namespace pipeline;Plan p;p.ready=true;p.enabled=Dlss;
    std::vector<Candidate> v={{{{1},10,Priority::Background},0},{{{2},5,Priority::Normal},Fsr},{{{1},20,Priority::FrameCritical},Dlss},{{{3},10,Priority::Normal},0},{{{4},100,Priority::FrameCritical},0},{{{5},10,Priority::Background},0}};
    GuardedPipelineCache cache({1});std::atomic<unsigned> calls{0};auto callback=[&](const Bytes& k,const Bytes* blob){++calls;if(k==Bytes{3})return ClassifiedAttempt{{AttemptStatus::Failed,{}},FailureClass::DeterministicUnsupported};return ClassifiedAttempt{{AttemptStatus::Created,blob?Bytes{}:Bytes{10}},FailureClass::None};};
    auto a=run(cache,v,p,2,30,{2,30},callback);
    check(a.plan.estimated_bytes==30&&a.plan.requests.size()==2&&a.plan.requests[0].key==Bytes{1});check(a.plan.rows[1]==Disposition::Disabled&&a.plan.rows[4]==Disposition::Deferred&&a.plan.rows[5]==Disposition::Deferred);check(a.plan.indices[0]==a.plan.indices[2]);check(calls==2&&a.batch.items[0].build.path==BuildPath::Cold&&a.batch.items[1].failure_remembered);
    auto b=run(cache,v,p,2,30,{2,30},callback);check(calls==3&&b.batch.items[0].build.path==BuildPath::Hit&&b.batch.items[1].status==BatchStatus::Suppressed);
    auto invalid=v;invalid.push_back({{{},1,Priority::Normal},0});invalid.push_back({{{9},1,Priority::Normal},128});
    const auto iv=select(invalid,p,2,30,30);check(iv.rows[6]==Disposition::Invalid&&iv.rows[7]==Disposition::Invalid);
    check(select(v,p,0,30,30).requests.empty());check(select(v,p,2,0,30).requests.empty());
}
void randomized(){using namespace pipeline;std::mt19937 rng(4070);for(unsigned t=0;t<10000;++t){Plan p;p.ready=true;p.enabled=Mask(rng()%128);std::vector<Candidate> v;std::size_t jobs=rng()%9,bytes=rng()%200,wave=1+rng()%60;
    for(unsigned i=0;i<32;++i){v.push_back({{{static_cast<std::uint8_t>(rng()%16)},1+rng()%50,static_cast<Priority>(rng()%3)},Mask(rng()%128)});}
    auto a=select(v,p,jobs,bytes,wave);check(a.requests.size()<=jobs&&a.estimated_bytes<=bytes);std::size_t sum=0;std::array<bool,16> seen{};
    for(const auto& x:a.requests){check(!seen[x.key[0]]&&x.estimated_bytes<=wave);seen[x.key[0]]=true;sum+=x.estimated_bytes;std::size_t estimate=0;auto priority=Priority::Background;
        for(const auto& c:v)if(c.request.key==x.key&&(c.required_features&p.enabled)==c.required_features){estimate=std::max(estimate,c.request.estimated_bytes);priority=promote_priority(priority,c.request.priority);}
        check(estimate==x.estimated_bytes&&priority==x.priority);}
    // Separate selection oracle: scan in descending priority, then first enabled occurrence.
    std::vector<Bytes> expected;std::size_t remaining=bytes;
    for(int level=2;level>=0;--level){for(std::size_t i=0;i<v.size();++i){const auto& c=v[i];if((c.required_features&p.enabled)!=c.required_features)continue;
        bool first=true;std::size_t estimate=0;int rank=0;
        for(std::size_t j=0;j<v.size();++j){const auto& d=v[j];if(d.request.key!=c.request.key||(d.required_features&p.enabled)!=d.required_features)continue;if(j<i)first=false;estimate=std::max(estimate,d.request.estimated_bytes);rank=std::max(rank,static_cast<int>(d.request.priority));}
        if(first&&rank==level&&expected.size()<jobs&&estimate<=wave&&estimate<=remaining){expected.push_back(c.request.key);remaining-=estimate;}
    }}
    check(expected.size()==a.requests.size());for(std::size_t i=0;i<expected.size();++i)check(expected[i]==a.requests[i].key);
    check(sum==a.estimated_bytes);for(std::size_t i=0;i<v.size();++i){if((v[i].required_features&p.enabled)!=v[i].required_features)check(a.rows[i]==Disposition::Disabled);else if(seen[v[i].request.key[0]])check(a.rows[i]==Disposition::Selected&&a.requests[a.indices[i]].key==v[i].request.key);else check(a.rows[i]==Disposition::Deferred);}
}std::cout<<"manifest_cases=10000 rows=320000 mismatches=0\n";}
int main(){capability_tests();integration();randomized();std::cout<<"cold/warm cache and deterministic refusal integration: PASS\n";}
