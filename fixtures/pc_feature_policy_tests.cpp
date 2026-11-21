#include "pc_feature_policy.hpp"
#include <iostream>
#include <stdexcept>
using namespace scarlett;
namespace {
void ensure(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
void no_renderer_must_not_fake_features() {
    Preferences w;w.neural=Neural::Dlss5;
    Capabilities c;c.vendor=Vendor::Nvidia;c.dlss_sr_ready=true;
    c.dlss_fg_ready=true;c.reflex_ready=true;c.dlss5_authorized_sdk_ready=true;
    c.dlss5_gpu_driver_approved=true;c.neural_scene_guides_ready=true;
    c.temporal_input_ready=true;c.frame_generation_input_ready=true;
    const auto r=resolve(w,c);
    ensure(!r.renderer_usable && r.backend==Backend::None,"invented renderer");
    ensure(r.upscaler==Upscaler::Native && r.frame_gen==FrameGen::Off &&
           r.neural==Neural::Off && r.latency==Latency::Engine,"features without renderer");
}
void nvidia_win() {
    Capabilities c;c.os=Os::Windows;c.vendor=Vendor::Nvidia;
    c.d3d12_renderer_ready=true;c.temporal_input_ready=true;
    c.frame_generation_input_ready=true;c.fsr_sr_ready=true;
    c.dlss_sr_ready=true;c.dlss_fg_ready=true;c.reflex_ready=true;
    c.directstorage_ready=true;
    const auto r=resolve(Preferences{},c);
    ensure(r.renderer_usable && r.backend==Backend::D3D12,"Windows D3D12");
    ensure(r.upscaler==Upscaler::Dlss && r.frame_gen==FrameGen::Dlss,"NVIDIA auto");
    ensure(r.latency==Latency::Reflex && r.storage==Storage::DirectStorage,"NVIDIA latency/storage");
    ensure(r.neural==Neural::Off,"DLSS5 must be opt in");
}
void linux_native() {
    Capabilities c;c.os=Os::Linux;c.vendor=Vendor::Amd;c.vulkan_renderer_ready=true;
    c.temporal_input_ready=true;c.frame_generation_input_ready=true;
    c.fsr_sr_ready=true;c.fsr_fg_ready=true;c.anti_lag2_ready=true;c.io_uring_ready=true;
    Preferences p;p.backend=Backend::D3D12;p.storage=Storage::DirectStorage;
    const auto wrong=resolve(p,c);
    ensure(!wrong.renderer_usable,"Linux D3D12 wrongly accepted as native");
    ensure(wrong.storage==Storage::Portable,"DirectStorage on Linux");
    p.backend=Backend::Auto;p.storage=Storage::Auto;
    const auto r=resolve(p,c);
    ensure(r.backend==Backend::Vulkan && r.upscaler==Upscaler::Fsr &&
           r.frame_gen==FrameGen::Fsr,"Linux Vulkan+FSR");
    ensure(r.storage==Storage::IoUring,"Linux io_uring");
    ensure(r.latency==Latency::AntiLag2,"synthetic declared AMD latency plugin");
}
void intel_and_fallbacks() {
    Capabilities c;c.os=Os::Windows;c.vendor=Vendor::Intel;c.d3d12_renderer_ready=true;
    c.temporal_input_ready=true;c.frame_generation_input_ready=true;
    c.xess_sr_ready=true;c.xess_fg_ready=true;c.xell_ready=true;
    const auto r=resolve(Preferences{},c);
    ensure(r.upscaler==Upscaler::Xess && r.frame_gen==FrameGen::Xess &&
           r.latency==Latency::Xell,"XeSS+XeFG+XeLL");
    c.temporal_input_ready=false;
    const auto no_mv=resolve(Preferences{},c);
    ensure(no_mv.upscaler==Upscaler::Native && no_mv.frame_gen==FrameGen::Off,
           "temporal effects enabled without motion vectors");
}
void user_requests_fail_closed() {
    Capabilities c;c.os=Os::Windows;c.vendor=Vendor::Amd;c.d3d12_renderer_ready=true;
    c.fsr_sr_ready=true;c.temporal_input_ready=true;
    Preferences p;p.upscaler=Upscaler::Dlss;p.frame_gen=FrameGen::Dlss;
    p.latency=Latency::Reflex;p.storage=Storage::IoUring;p.neural=Neural::Dlss5;
    const auto r=resolve(p,c);
    ensure(r.upscaler==Upscaler::Fsr,"unsupported SR no fallback");
    ensure(r.frame_gen==FrameGen::Off && r.latency==Latency::Engine &&
           r.storage==Storage::Portable && r.neural==Neural::Off,"unsupported modules enabled");
    ensure(r.reasons.size()>=4,"unsupported requests not explained");
}
void no_silent_fg_replacement() {
    Capabilities c;c.os=Os::Windows;c.vendor=Vendor::Nvidia;c.d3d12_renderer_ready=true;
    c.temporal_input_ready=true;c.frame_generation_input_ready=true;c.fsr_fg_ready=true;
    Preferences p;p.frame_gen=FrameGen::Dlss;
    ensure(resolve(p,c).frame_gen==FrameGen::Off,"explicit DLSS-FG switched to AMD silently");
}
void neural_gate_is_distinct() {
    Capabilities c;c.os=Os::Windows;c.vendor=Vendor::Nvidia;
    c.d3d12_renderer_ready=true;c.temporal_input_ready=true;
    c.dlss5_gpu_driver_approved=true;c.neural_scene_guides_ready=true;
    Preferences p;p.neural=Neural::Dlss5;
    ensure(resolve(p,c).neural==Neural::Off,"unauthorized DLSS5 activated");
    c.dlss5_authorized_sdk_ready=true;
    const auto yes=resolve(p,c);
    ensure(yes.neural==Neural::Dlss5 && yes.upscaler==Upscaler::Native,
           "DLSS5 neural incorrectly confused with SR/FG");
    c.os=Os::Linux;c.d3d12_renderer_ready=false;c.vulkan_renderer_ready=true;
    ensure(resolve(p,c).neural==Neural::Off,"Linux DLSS5 selected without verified SDK");
}
void manual_windows_vulkan() {
    Capabilities c;c.os=Os::Windows;c.vulkan_renderer_ready=true;
    Preferences p;p.backend=Backend::Vulkan;
    const auto r=resolve(p,c);
    ensure(r.backend==Backend::Vulkan && r.renderer_usable,"native Windows Vulkan disabled");
}
void fsr_without_fg_hooks() {
    Capabilities c;c.os=Os::Windows;c.vendor=Vendor::Amd;c.d3d12_renderer_ready=true;
    c.temporal_input_ready=true;c.fsr_sr_ready=true;c.fsr_fg_ready=true;
    const auto r=resolve(Preferences{},c);
    ensure(r.upscaler==Upscaler::Fsr && r.frame_gen==FrameGen::Off,"FG enabled without pacing/hooks");
}
}
int main() {
    try {
        no_renderer_must_not_fake_features();
        nvidia_win();
        linux_native();
        intel_and_fallbacks();
        user_requests_fail_closed();
        no_silent_fg_replacement();
        neural_gate_is_distinct();
        manual_windows_vulkan();
        fsr_without_fg_hooks();
    }catch(const std::exception& e) {
        std::cerr<<"native PC feature policy FAILED: "<<e.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"source-built-native-PC-feature-policy\","
             <<"\"scenarios\":9,\"windows_linux\":true,"
             <<"\"safe_fallbacks\":true,\"provider_sdk_executed\":false,"
             <<"\"console_game_tested\":false}\n";
    return 0;
}
