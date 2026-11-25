#include "native_audio_ring.hpp"
#include <array>
#include <deque>
#include <iostream>
#include <limits>
#include <random>
#include <thread>
using scarlett::audio::PcmRing;
void check(bool b){if(!b)throw std::runtime_error("PCM ring mismatch");}
void model(unsigned seed){
    std::mt19937 rng(seed);
    PcmRing ring(64,std::numeric_limits<std::uint32_t>::max()-16u);
    std::deque<float> expected;
    std::array<float,130> input{},output{};
    std::uint64_t delivered=0,silence=0,rejected=0;
    for(unsigned i=0;i<100000u;++i){
        const auto n=static_cast<std::uint32_t>(rng()%66u);
        if(rng()%2u){
            for(auto& v:input)v=static_cast<float>(rng()%1000u)/1000.0f;
            const bool ok=n<=64u && expected.size()+2u*n<=128u;
            check(ring.push(input.data(),n)==ok);
            if(ok)for(std::size_t j=0;j<2u*n;++j)expected.push_back(input[j]);
            else ++rejected;
        }else{
            output.fill(-99.0f);
            const auto got=ring.pull(output.data(),n);
            if(n>64u){check(!got.valid && output[0]==-99.0f);continue;}
            const auto count=static_cast<std::uint32_t>(std::min<std::size_t>(n,expected.size()/2u));
            check(got.valid && got.delivered==count && got.silence==n-count);
            for(std::size_t j=0;j<2u*count;++j){check(output[j]==expected.front());expected.pop_front();}
            for(std::size_t j=2u*count;j<2u*n;++j)check(output[j]==0.0f);
            for(std::size_t j=2u*n;j<output.size();++j)check(output[j]==-99.0f);
            delivered+=got.delivered;silence+=got.silence;
        }
    }
    std::cout<<"seed="<<seed<<" operations=100000 delivered="<<delivered<<" silence="<<silence<<" rejected="<<rejected<<" mismatches=0\n";
}
void concurrent(){
    constexpr std::uint32_t total=1000000;
    PcmRing ring(1024,std::numeric_limits<std::uint32_t>::max()-100u);
    std::atomic<bool> failed{false};
    std::thread producer([&]{
        std::array<float,126> in{};
        for(std::uint32_t pos=0;pos<total && !failed.load();){
            const auto n=std::min(63u,total-pos);
            for(std::uint32_t j=0;j<n;++j){in[2u*j]=static_cast<float>(pos+j+1u);in[2u*j+1u]=-in[2u*j];}
            if(ring.push(in.data(),n))pos+=n;
            else std::this_thread::yield();
        }
    });
    std::array<float,142> out{};
    std::uint32_t pos=0;
    while(pos<total && !failed.load()){
        const auto result=ring.pull(out.data(),std::min(71u,total-pos));
        if(!result.valid){failed.store(true);break;}
        for(std::uint32_t j=0;j<result.delivered;++j){
            if(out[2u*j]!=static_cast<float>(pos+j+1u)||out[2u*j+1u]!=-out[2u*j])failed.store(true);
        }
        for(std::uint32_t j=result.delivered;j<result.delivered+result.silence;++j)if(out[2u*j]!=0||out[2u*j+1u]!=0)failed.store(true);
        pos+=result.delivered;
        if(!result.delivered)std::this_thread::yield();
    }
    producer.join();check(!failed.load() && pos==total);
    std::cout<<"concurrent_frames="<<total<<" stereo_samples="<<2u*total<<" mismatches=0\n";
}
int main(){
    for(auto n:{0u,3u,65537u}){bool rejected=false;try{PcmRing x(n);}catch(const std::invalid_argument&){rejected=true;}check(rejected);}
    PcmRing x(1);float v[2]{0.25f,-0.25f},o[2]{};
    check(x.push(nullptr,0)&&!x.push(nullptr,1)&&!x.pull(nullptr,1).valid&&x.pull(nullptr,0).valid);
    check(x.push(v,1)&&!x.push(v,1)&&x.pull(o,1).delivered==1&&o[0]==v[0]&&o[1]==v[1]);
    check(x.pull(o,1).silence==1&&o[0]==0&&o[1]==0);
    for(unsigned s:{64u,4070u,20261125u})model(s);
    concurrent();
}
