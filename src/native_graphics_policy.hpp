#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
namespace scarlett::graphics {
enum class Backend {D3D12,Vulkan};
enum class Mode {Disabled,Raster,RayTracing};
enum class Status {Ready,BackendUnavailable,UnsupportedEffect,Invalid};
enum class Request {Off,PreferRaster,PreferRT};
// Effects: reflections, shadows, indirect lighting. Mandatory effects cannot
// disappear. A raster implementation must be confirmed by the caller, not
// inferred from API support. No silent RT fallback when the user requests off.
struct Effect {bool required=false,raster=false,ray=false;};
struct Capabilities {bool native_backend=false,hardware=false,ray_tracing=false;std::array<Effect,3> effects{};};
struct Settings {Backend backend=Backend::D3D12;bool allow_rt=false;std::array<Request,3> requests{Request::PreferRaster,Request::PreferRaster,Request::PreferRaster};};
struct Plan {Status status=Status::Invalid;std::array<Mode,3> modes{};bool build_acceleration_structures=false;unsigned unresolved=0;};
inline Plan resolve(const Settings& settings,const Capabilities& caps) noexcept {
    Plan p;
    if(settings.backend!=Backend::D3D12&&settings.backend!=Backend::Vulkan)return p;
    for(auto r:settings.requests)if(r!=Request::Off&&r!=Request::PreferRaster&&r!=Request::PreferRT)return p;
    if(!caps.native_backend||!caps.hardware){p.status=Status::BackendUnavailable;return p;}
    for(unsigned i=0;i<3;++i){const auto e=caps.effects[i];const auto r=settings.requests[i];
        const bool rt=settings.allow_rt&&caps.ray_tracing&&e.ray;
        if(r==Request::Off){if(e.required)p.unresolved|=1u<<i;continue;}
        if(r==Request::PreferRT&&rt)p.modes[i]=Mode::RayTracing;
        else if(e.raster)p.modes[i]=Mode::Raster;
        else if(rt)p.modes[i]=Mode::RayTracing;
        else if(e.required)p.unresolved|=1u<<i;
    }
    if(p.unresolved){p.status=Status::UnsupportedEffect;p.modes={};return p;}
    p.status=Status::Ready;
    p.build_acceleration_structures=std::any_of(p.modes.begin(),p.modes.end(),[](Mode m){return m==Mode::RayTracing;});return p;
}
// All inputs are bytes from one coherent sample of one memory segment.
// usage includes tracked allocations. pending means allocations NOT reflected
// in usage yet. Never count freed bytes until observed; no memory reservation
// or allocation is performed by this function. Budget zero => no admission.
struct MemorySample {std::uint64_t budget=0,usage=0,pending=0,reserve=0;};
constexpr std::uint64_t additional_bytes(MemorySample s) noexcept {
    auto left=s.budget;for(auto n:{s.usage,s.pending,s.reserve}){if(n>=left)return 0;left-=n;}return left;
}
// Clamp optional streaming by independent GPU, RAM and IO constraints.
// These are caller-computed caps in bytes for one admission window, not
// interchangeable resource units or actual frame-time estimates.
constexpr std::uint64_t stream_admission(MemorySample gpu,MemorySample ram,std::uint64_t io,std::uint64_t requested) noexcept {
    return std::min(std::min(additional_bytes(gpu),additional_bytes(ram)),std::min(io,requested));
}
}
