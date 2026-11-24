#include "native_pipeline_batch.hpp"
#include <iostream>
#include <stdexcept>
using namespace scarlett::pipeline;
void check(bool b){if(!b)throw std::runtime_error("priority integration invariant failed");}
int main(){
    using P=Priority;
    Cache c({1},4096,64);
    std::vector<BatchRequest> r{{{1},4,P::Background},{{2},4,P::Normal},{{3},4,P::FrameCritical},{{1},6,P::FrameCritical},{{4},4,P::Normal},{{5},4,static_cast<P>(255)},{{6},100,P::FrameCritical}};
    Bytes order;
    auto result=prepare_batch(c,r,[&](const Bytes& key,const Bytes* b){check(!b);order.push_back(key.at(0));return Attempt{AttemptStatus::Created,{key.at(0)}};},{1,8});
    check(order==Bytes({1,3,2,4}));check(c.size()==4);check(result.unique_jobs==5);check(result.peak_estimated_bytes==6);
    check(result.items[0].build.path==BuildPath::Cold&&!result.items[0].coalesced);check(result.items[3].coalesced&&result.items[3].build.path==BuildPath::Cold);
    check(result.items[5].status==BatchStatus::Invalid&&result.items[6].status==BatchStatus::BudgetExceeded);
    order.clear();auto warm=prepare_batch(c,r,[&](const Bytes& key,const Bytes* b){check(b&&*b==key);order.push_back(key.at(0));return Attempt{AttemptStatus::Created,{}};},{1,8});
    check(order==Bytes({1,3,2,4}));check(warm.items[3].build.path==BuildPath::Hit);
    order.clear();auto defaults=prepare_batch(c,{{{4},4},{{2},4}},[&](const Bytes& key,const Bytes* b){check(b);order.push_back(key.at(0));return Attempt{AttemptStatus::Created,{}};},{1,8});
    check(order==Bytes({4,2})&&defaults.items.size()==2);
    std::cout<<"priority integration: promotion, stable admission, result mapping, rejection, cold/warm, defaults PASS\n";
}
