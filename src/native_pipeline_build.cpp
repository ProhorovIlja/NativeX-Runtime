#include "native_pipeline_build.hpp"
namespace scarlett::pipeline {
namespace {
void u32(Bytes& b,std::uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(n>>(8u*i)));}
}
Bytes compute_key(const Bytes& root,const Bytes& shader,std::uint32_t node_mask,std::uint32_t flags){
    constexpr std::size_t overhead=24;
    if(root.empty()||shader.empty()||root.size()>MaxKeyBytes-overhead||
       shader.size()>MaxKeyBytes-overhead-root.size())return {};
    Bytes key{'N','X','C','S',1,0,0,0};
    key.reserve(overhead+root.size()+shader.size());
    u32(key,node_mask);u32(key,flags);u32(key,static_cast<std::uint32_t>(root.size()));
    u32(key,static_cast<std::uint32_t>(shader.size()));
    key.insert(key.end(),root.begin(),root.end());key.insert(key.end(),shader.begin(),shader.end());
    return key;
}
BuildReport create_cached(Cache& cache,const Bytes& key,const CreateOperation& create){
    if(key.empty()||key.size()>MaxKeyBytes||!create)return {};
    BuildReport report{BuildPath::Failed,WriteStatus::NotAttempted,0,false};
    if(const auto* blob=cache.find(key)){
        ++report.attempts;
        const auto attempt=create(blob);
        if(attempt.status==AttemptStatus::Created){report.path=BuildPath::Hit;return report;}
        if(attempt.status!=AttemptStatus::CacheRejected)return report;
        cache.erase(key);report.cache_rejected=true;
    }
    ++report.attempts;
    const auto fresh=create(nullptr);
    if(fresh.status!=AttemptStatus::Created)return report;
    report.path=report.cache_rejected?BuildPath::Rebuilt:BuildPath::Cold;
    if(fresh.fresh_blob.empty())report.write=WriteStatus::Unavailable;
    else report.write=cache.put(key,fresh.fresh_blob)==Status::Ok?WriteStatus::Stored:WriteStatus::Rejected;
    return report;
}
} // namespace scarlett::pipeline
