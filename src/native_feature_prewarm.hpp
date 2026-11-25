#pragma once
#include "native_graphics_policy.hpp"
#include "native_pipeline_failure.hpp"
#include <array>
#include <map>
namespace scarlett::features {
// Capabilities refer to one initialized device, API, driver and renderer build.
// No SDK loading, hardware probing or assumption based on vendor ID alone.
enum class Upscaler {Native,DLSS,FSR,XeSS};
enum class Storage {Win32,DirectStorageCPU,DirectStorageGPU};
enum class Vendor {Other,Nvidia,Amd,Intel};
using Mask=std::uint32_t;
constexpr Mask Dlss=1u,Fsr=2u,Xess=4u,Reflex=8u,Ray=16u,DsGpu=32u,DsCpu=64u;
constexpr Mask All=Dlss|Fsr|Xess|Reflex|Ray|DsGpu|DsCpu;
constexpr Mask Color=1u,Depth=2u,Motion=4u,Jitter=8u,Exposure=16u,HistoryReset=32u;
constexpr Mask Temporal=Color|Depth|Motion|Jitter|Exposure|HistoryReset;
struct Support {bool sdk_ready=false,device_verified=false;Mask inputs=0;};
struct Caps {
    graphics::Backend backend=graphics::Backend::D3D12;
    Vendor vendor=Vendor::Other;
    std::array<Support,3> sr{}; // DLSS, FSR, XeSS, confirmed for selected backend
    Mask available_inputs=0;
    bool reflex_ready=false,reflex_markers=false;
    bool ds_cpu_verified=false,ds_gpu_verified=false,win32_verified=false;
};
struct Settings {
    Upscaler requested=Upscaler::Native;
    bool allow_native_fallback=true,reflex=false,directstorage=true,gpu_decode=true;
};
struct Plan {bool ready=false;Upscaler sr=Upscaler::Native;Storage storage=Storage::Win32;Mask enabled=0;bool sr_fallback=false;};
inline Plan resolve(const graphics::Settings& render,const graphics::Plan& base,const Settings& s,const Caps& c) noexcept {
    Plan p;
    if(base.status!=graphics::Status::Ready||render.backend!=c.backend||
       (c.backend!=graphics::Backend::D3D12&&c.backend!=graphics::Backend::Vulkan)||
       (c.vendor!=Vendor::Other&&c.vendor!=Vendor::Nvidia&&c.vendor!=Vendor::Amd&&c.vendor!=Vendor::Intel))return p;
    const auto idx=static_cast<unsigned>(s.requested);
    if(idx>3)return p;
    if(idx){const auto& v=c.sr[idx-1];
        const bool valid=v.sdk_ready&&v.device_verified&&(v.inputs&Temporal)==Temporal&&
          (v.inputs&c.available_inputs)==v.inputs&&(idx!=1||c.vendor==Vendor::Nvidia);
        if(valid){p.sr=s.requested;p.enabled|=Mask{1}<<(idx-1);}
        else if(s.allow_native_fallback)p.sr_fallback=true;
        else return {};
    }
    if(s.reflex&&c.vendor==Vendor::Nvidia&&c.reflex_ready&&c.reflex_markers)p.enabled|=Reflex;
    if(base.build_acceleration_structures)p.enabled|=Ray;
    // GPU destination/codec support must be confirmed on the current path.
    // Do not infer Vulkan GPU decompression interop from D3D12 support.
    if(s.directstorage&&s.gpu_decode&&c.ds_gpu_verified){p.storage=Storage::DirectStorageGPU;p.enabled|=DsGpu;}
    else if(s.directstorage&&c.ds_cpu_verified){p.storage=Storage::DirectStorageCPU;p.enabled|=DsCpu;}
    else if(!c.win32_verified)return {};
    p.ready=true;return p;
}
// Finite, explicitly invoked prewarm. Manifest uses canonical pipeline keys
// and feature requirements from the caller's verified renderer. Shared keys
// are merged BEFORE admission. Disabled feature rows never enter the cache.
struct Candidate {pipeline::BatchRequest request;Mask required_features=0;};
enum class Disposition {Invalid,Disabled,Deferred,Selected};
struct WarmPlan {std::vector<pipeline::BatchRequest> requests;std::vector<Disposition> rows;std::vector<std::size_t> indices;std::size_t estimated_bytes=0;};
inline WarmPlan select(const std::vector<Candidate>& input,const Plan& p,std::size_t max_jobs,std::size_t total_bytes,std::size_t wave_bytes){
    constexpr std::size_t MaxKeys=16u*1024u*1024u,MaxWave=2ull*1024u*1024u*1024u;
    if(!p.ready||(p.enabled&~All)||input.size()>4096||max_jobs>4096||!wave_bytes||wave_bytes>MaxWave)
        throw std::invalid_argument("invalid prewarm limits or plan");
    std::size_t keys=0;for(const auto& c:input){if(c.request.key.size()>MaxKeys-keys)throw std::invalid_argument("manifest key limit");keys+=c.request.key.size();}
    WarmPlan out;out.rows.resize(input.size(),Disposition::Invalid);out.indices.resize(input.size(),input.size());
    std::map<pipeline::Bytes,std::size_t> ids;std::vector<pipeline::BatchRequest> unique;std::vector<std::size_t> mapping(input.size(),input.size());
    for(std::size_t i=0;i<input.size();++i){const auto& c=input[i];const auto& r=c.request;
        if((c.required_features&~All)||r.key.empty()||r.key.size()>pipeline::MaxKeyBytes||!r.estimated_bytes||!pipeline::valid_priority(r.priority))continue;
        if((c.required_features&p.enabled)!=c.required_features){out.rows[i]=Disposition::Disabled;continue;}
        out.rows[i]=Disposition::Deferred;
        const auto j=ids.emplace(r.key,unique.size());mapping[i]=j.first->second;
        if(j.second)unique.push_back(r);
        else{auto& u=unique[j.first->second];u.estimated_bytes=std::max(u.estimated_bytes,r.estimated_bytes);u.priority=pipeline::promote_priority(u.priority,r.priority);}
    }
    std::vector<pipeline::Priority> priorities;for(const auto& r:unique)priorities.push_back(r.priority);
    std::vector<std::size_t> admitted(unique.size(),input.size());
    for(auto i:pipeline::priority_order(priorities)){const auto& r=unique[i];
        if(out.requests.size()==max_jobs)break;
        if(r.estimated_bytes>wave_bytes||r.estimated_bytes>total_bytes-out.estimated_bytes)continue;
        admitted[i]=out.requests.size();out.estimated_bytes+=r.estimated_bytes;out.requests.push_back(r);
    }
    for(std::size_t i=0;i<input.size();++i)if(mapping[i]!=input.size()&&admitted[mapping[i]]!=input.size()){
        out.rows[i]=Disposition::Selected;out.indices[i]=admitted[mapping[i]];
    }
    return out;
}
struct WarmResult {WarmPlan plan;pipeline::BatchResult batch;};
inline WarmResult run(pipeline::GuardedPipelineCache& cache,const std::vector<Candidate>& candidates,const Plan& plan,
                      std::size_t jobs,std::size_t bytes,pipeline::BatchLimits limits,const pipeline::ClassifiedBatchCreate& create){
    auto selected=select(candidates,plan,jobs,bytes,limits.estimated_bytes);
    auto batch=pipeline::prepare_guarded_batch(cache,selected.requests,create,limits);
    return {std::move(selected),std::move(batch)};
}
}
