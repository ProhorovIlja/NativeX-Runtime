#include "native_pipeline_diagnostics.hpp"
#include "native_pipeline_failure.hpp"
#include <iostream>
#include <stdexcept>
using namespace scarlett::pipeline;
int main(){
    GuardedPipelineCache c({1});
    auto unsupported=[](const Bytes*){return ClassifiedAttempt{{AttemptStatus::Failed,{}},FailureClass::DeterministicUnsupported};};
    c.create({1},unsupported);
    c.create({2},[](const Bytes*){return ClassifiedAttempt{{AttemptStatus::Created,{42}},FailureClass::None};});
    const auto batch=prepare_guarded_batch(c,{{{1},8},{{2},8},{{3},8},{{3},8},{{4},8},{{5},99},{{},8},{{6},8}},
        [](const Bytes& k,const Bytes* cached)->ClassifiedAttempt{
            if(k[0]==2&&cached)return {{AttemptStatus::Created,{}},FailureClass::None};
            if(k[0]==3)return {{AttemptStatus::Failed,{}},FailureClass::DeterministicUnsupported};
            if(k[0]==4)throw std::runtime_error("private details not exported");
            return {{AttemptStatus::Created,{7}},FailureClass::None};
        },{2,16});
    std::cout<<batch_diagnostics_json(batch)<<'\n'<<batch_diagnostics_json(batch,2)<<'\n'<<batch_diagnostics_json(batch,0)<<'\n';
    BatchResult big;big.items.resize(5000);big.items[0].status=static_cast<BatchStatus>(99);
    std::cout<<batch_diagnostics_json(big,99999)<<'\n'<<batch_diagnostics_json({})<<'\n';
}
