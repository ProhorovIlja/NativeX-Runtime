#include "native_pipeline_priority.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace scarlett::pipeline;
void check(bool b){if(!b)throw std::runtime_error("priority invariant failed");}
int main(){
    using P=Priority;
    check(priority_order({}).empty());
    check(priority_order({P::Background,P::Normal,P::FrameCritical,P::Normal,P::FrameCritical})==std::vector<std::size_t>({2,4,1,3,0}));
    check(promote_priority(P::Background,P::FrameCritical)==P::FrameCritical);
    check(promote_priority(P::FrameCritical,P::Normal)==P::FrameCritical);
    check(!valid_priority(static_cast<P>(255)));
    std::mt19937 rng(42);
    std::size_t count=0;
    for(unsigned round=0;round<1000;++round){
        std::vector<P> p(rng()%4097);
        for(auto& x:p)x=static_cast<P>(rng()%3);
        auto actual=priority_order(p);
        std::vector<std::size_t> expected;
        for(auto level:{P::FrameCritical,P::Normal,P::Background})
            for(std::size_t i=0;i<p.size();++i)if(p[i]==level)expected.push_back(i);
        check(actual==expected);count+=p.size();
    }
    std::cout<<"priority_cases=1000 jobs="<<count<<" errors=0\n";
}
