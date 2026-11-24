#include "native_wave64_reference.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace scarlett::wave;
void check(bool b){if(!b)throw std::runtime_error("wave mismatch");}
bool bit(Mask m,unsigned n){return n<64 && ((m>>n)&1u)!=0;}
void compare(Mask mask,Mask pred,const Values& v,std::mt19937_64& rng){
    auto e=SplitMask::from(mask),p=SplitMask::from(pred);
    check(e.joined()==mask);check(ballot(e,p)==(mask&pred));
    std::optional<unsigned> first;std::uint32_t total=0;
    for(unsigned i=0;i<64;++i)if(bit(mask,i)){if(!first)first=i;total+=v[i];}
    check(first_lane(e)==first);
    unsigned count=0;std::uint32_t sum=0;
    for(unsigned i=0;i<=64;++i){
        bool active=bit(mask,i);check(e.active(i)==active);
        check(prefix_count(e,p,i)==(active?std::optional<unsigned>{count}:std::nullopt));
        check(prefix_sum(v,e,i)==(active?std::optional<std::uint32_t>{sum}:std::nullopt));
        check(active_sum(v,e,i)==(active?std::optional<std::uint32_t>{total}:std::nullopt));
        for(unsigned src:{0u,31u,32u,63u,64u,static_cast<unsigned>(rng()%65)}){
            std::optional<std::uint32_t> expected;
            if(active&&bit(mask,src))expected=v[src];
            check(read_lane(v,e,i,src)==expected);
        }
        if(active){sum+=v[i];if(bit(pred,i))++count;}
    }
}
int main(){
    std::mt19937_64 rng(0x64);Values v{};v.fill(UINT32_MAX);
    for(Mask mask:{Mask{0},~Mask{0},Mask{1}<<31,Mask{1}<<32,Mask{1}<<63,Mask{0xffffffff},Mask{0xffffffff00000000ull},Mask{0x8000000180000001ull}}){
        compare(mask,~Mask{0},v,rng);compare(mask,0,v,rng);
    }
    for(unsigned n=0;n<10000;++n){for(auto& x:v)x=static_cast<std::uint32_t>(rng());const auto mask=rng();const auto pred=rng();compare(mask,pred,v,rng);}
    std::cout<<"wave_cases=10016 lane_queries=651040 cross_lane_reads=3906240 mismatches=0\n";
}
