// Conservative compatibility identity for local D3D12 cached pipeline blobs.
#pragma once
#include "native_pipeline_cache.hpp"
namespace scarlett::pipeline {
struct ScopeIdentity {
    std::uint32_t vendor=0,device=0,subsystem=0,revision=0;
    std::uint32_t luid_low=0,luid_high=0,node_count=0;
    bool software=false;
    std::uint64_t driver=0,d3d12=0,dxgi=0,compiler=0;
    bool core_loaded=false;
    std::uint64_t core=0;
    Bytes producer; // explicit application/translator build identity
};
// Empty if required evidence is absent. LUID is deliberately included: a
// reboot or adapter reset may cause a cache miss, even with identical hardware.
Bytes compatibility_scope(const ScopeIdentity& identity);
} // namespace scarlett::pipeline
