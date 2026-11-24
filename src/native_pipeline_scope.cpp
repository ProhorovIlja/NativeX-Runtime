#include "native_pipeline_scope.hpp"
namespace scarlett::pipeline {
namespace {
void append(Bytes& b,std::uint64_t n,unsigned width){for(unsigned i=0;i<width;++i)b.push_back(static_cast<std::uint8_t>(n>>(8u*i)));}
}
Bytes compatibility_scope(const ScopeIdentity& x){
    if(!x.vendor||!x.device||(!x.luid_low&&!x.luid_high)||!x.node_count||x.node_count>32||
       !x.driver||!x.d3d12||!x.dxgi||!x.compiler||x.core_loaded!=(x.core!=0)||
       x.producer.empty()||x.producer.size()>1024)return {};
    Bytes out{'N','X','S','C',1,0,0,0};
    out.reserve(82+x.producer.size());
    for(auto v:{x.vendor,x.device,x.subsystem,x.revision,x.luid_low,x.luid_high,x.node_count})append(out,v,4);
    append(out,x.software?1u:0u,1);append(out,x.core_loaded?1u:0u,1);
    for(auto v:{x.driver,x.d3d12,x.dxgi,x.compiler,x.core})append(out,v,8);
    append(out,x.producer.size(),4);out.insert(out.end(),x.producer.begin(),x.producer.end());
    return out;
}
} // namespace scarlett::pipeline
