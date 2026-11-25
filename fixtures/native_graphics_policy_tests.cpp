#include "native_graphics_policy.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <random>
using namespace scarlett::graphics;
void check(bool b){if(!b)throw std::runtime_error("graphics policy mismatch");}
int main(){
    unsigned cases=0;
    for(unsigned mask=0;mask<4096;++mask){
        Capabilities c{true,true,bool(mask&1u),{}};Settings s;s.allow_rt=bool(mask&2u);s.backend=(mask&4u)?Backend::Vulkan:Backend::D3D12;
        for(unsigned i=0;i<3;++i){unsigned b=mask>>(3+3*i);c.effects[i]={bool(b&1u),bool(b&2u),bool(b&4u)};}
        for(unsigned requests=0;requests<27;++requests){unsigned n=requests;for(auto& r:s.requests){r=static_cast<Request>(n%3);n/=3;}
            auto p=resolve(s,c);unsigned missing=0;bool rt=false;
            for(unsigned i=0;i<3;++i){auto e=c.effects[i];auto r=s.requests[i];bool usable_rt=s.allow_rt&&c.ray_tracing&&e.ray;
                if(e.required&&(r==Request::Off||(!e.raster&&!usable_rt)))missing|=1u<<i;
            }
            check(p.unresolved==missing);check(p.status==(missing?Status::UnsupportedEffect:Status::Ready));
            for(unsigned i=0;i<3;++i){auto m=p.modes[i];auto e=c.effects[i];auto r=s.requests[i];bool usable_rt=s.allow_rt&&c.ray_tracing&&e.ray;
                if(missing||r==Request::Off)check(m==Mode::Disabled);
                else if(r==Request::PreferRT&&usable_rt)check(m==Mode::RayTracing);
                else if(e.raster)check(m==Mode::Raster);
                else if(usable_rt)check(m==Mode::RayTracing);
                else check(m==Mode::Disabled);
                rt|=m==Mode::RayTracing;
            }
            check(p.build_acceleration_structures==rt);if(!s.allow_rt)check(!rt);++cases;
        }
    }
    Settings s;Capabilities c;check(resolve(s,c).status==Status::BackendUnavailable);c.native_backend=true;check(resolve(s,c).status==Status::BackendUnavailable);c.hardware=true;s.requests[0]=static_cast<Request>(9);check(resolve(s,c).status==Status::Invalid);
    const auto max=std::numeric_limits<std::uint64_t>::max();check(additional_bytes({max,max,1,1})==0);check(additional_bytes({max,max-3,1,1})==1);check(additional_bytes({0,0,0,0})==0);check(additional_bytes({100,40,20,10})==30);
    std::mt19937_64 rng(4070);for(unsigned i=0;i<100000;++i){MemorySample g{rng()%1000,rng()%1000,rng()%1000,rng()%1000},r{rng()%1000,rng()%1000,rng()%1000,rng()%1000};auto io=rng()%1000,requested=rng()%1000;auto ref=[](MemorySample a){auto spent=a.usage+a.pending+a.reserve;return spent>=a.budget?0:a.budget-spent;};check(stream_admission(g,r,io,requested)==std::min({ref(g),ref(r),io,requested}));}
    std::cout<<"policy_cases="<<cases<<" memory_cases=100000 mismatches=0\n";
}
