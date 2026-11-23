// Standalone source-built policy tests. Game setting hooks are synthetic only.
#include "native_1080p60_profile.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace scarlett;
using namespace scarlett::tuning;
namespace {
void ensure(bool condition,const char* why) {
    if(!condition)throw std::runtime_error(why);
}
constexpr std::size_t index(Knob k){
    return static_cast<std::size_t>(k);
}
Capabilities nvidia_1080p() {
    Capabilities c{};
    c.os=Os::Windows;
    c.vendor=Vendor::Nvidia;
    c.d3d12_renderer_ready=true;
    c.temporal_input_ready=true;
    c.dlss_sr_ready=true;
    c.reflex_ready=true;
    c.directstorage_ready=true;
    c.d3d12.dlss_sr=true;
    c.d3d12.reflex=true;
    return c;
}
GameHooks every_verified_hook() {
    GameHooks h{};
    h.verified.fill(true);
    return h;
}
void empty_support_never_claims_game_mods() {
    const auto p=propose_1080p60(Capabilities{},GameHooks{});
    ensure(p.output_width==1920 && p.output_height==1080 &&
           p.target_native_fps==60 && !p.fps_measured &&
           !p.real_title_tested,"inferred actual title performance");
    ensure(!p.pc.renderer_usable && p.pc.frame_gen==FrameGen::Off &&
           p.pc.neural==Neural::Off && p.upscaling==UpscaleMode::Native,
           "unsupported graphics or FG enabled");
    for(const auto& s:p.settings)
        ensure(!s.accepted && !s.hook_verified &&
               s.eligible==Level::Unchanged,"fake game hook invented");
}
void verified_dlss_quality_and_reflex_without_fg() {
    auto c=nvidia_1080p();
    c.dlss_fg_ready=true;
    c.frame_generation_input_ready=true;
    c.d3d12.dlss_fg=true; // deliberately offered, but user forbids FG
    c.dlss5_authorized_sdk_ready=true;
    c.dlss5_gpu_driver_approved=true;
    c.neural_scene_guides_ready=true;
    c.d3d12.neural_dlss5=true; // deliberately offered, but not requested
    const auto p=propose_1080p60(c,GameHooks{});
    ensure(p.pc.backend==Backend::D3D12 &&
           p.pc.upscaler==Upscaler::Dlss &&
           p.upscaling==UpscaleMode::Quality &&
           p.pc.latency==Latency::Reflex &&
           p.pc.storage==Storage::DirectStorage &&
           p.pc.frame_gen==FrameGen::Off && p.pc.neural==Neural::Off,
           "no-FG and no-DLSS5 policy violated on RTX-class host");
}
void balanced_opt_in_and_no_unintegrated_dlss() {
    const auto c=nvidia_1080p();
    Goals g{};
    g.preferred_upscaling=UpscaleMode::Balanced;
    ensure(propose_1080p60(c,GameHooks{},g).upscaling==
           UpscaleMode::Balanced,"Balanced mode manual choice ignored");
    g.preferred_upscaling=UpscaleMode::Native;
    ensure(propose_1080p60(c,GameHooks{},g).upscaling==
           UpscaleMode::Native,"native-resolution override ignored");
    g.preferred_upscaling=UpscaleMode::Balanced;
    auto unintegrated=c;
    unintegrated.d3d12.dlss_sr=false;
    const auto off=propose_1080p60(unintegrated,GameHooks{},g);
    ensure(off.pc.upscaler==Upscaler::Native &&
           off.upscaling==UpscaleMode::Native,
           "unintegrated DLSS was represented as installed");
    unintegrated.d3d12.dlss_sr=true;
    unintegrated.temporal_input_ready=false;
    ensure(propose_1080p60(unintegrated,GameHooks{},g).upscaling==
           UpscaleMode::Native,"temporal input prerequisites ignored");
}
void ray_tracing_cannot_be_disabled_without_real_raster_path() {
    auto h=every_verified_hook();
    auto p=propose_1080p60(nvidia_1080p(),h);
    const auto rt=p.settings[index(Knob::RayTracing)];
    ensure(!rt.accepted && rt.requested==Level::Off &&
           rt.eligible==Level::Unchanged &&
           !p.raster_only_eligible,
           "unverified mandatory RT replacement declared completed");
    h.non_rt_renderer_complete=true;
    p=propose_1080p60(nvidia_1080p(),h);
    ensure(p.raster_only_eligible &&
           p.settings[index(Knob::RayTracing)].accepted &&
           p.settings[index(Knob::RayTracing)].eligible==Level::Off,
           "verified raster path incorrectly rejected");
}
void real_settings_only_when_hook_is_verified() {
    GameHooks h{};
    h.verified[index(Knob::Shadows)]=true;
    h.verified[index(Knob::MotionBlur)]=true;
    const auto p=propose_1080p60(nvidia_1080p(),h);
    ensure(p.settings[index(Knob::Shadows)].eligible==Level::Low &&
           p.settings[index(Knob::MotionBlur)].eligible==Level::Off &&
           p.settings[index(Knob::MotionBlur)].accepted,
           "verified graphics controls unavailable");
    ensure(!p.settings[index(Knob::FilmGrain)].accepted &&
           p.settings[index(Knob::FilmGrain)].requested==Level::Off &&
           p.settings[index(Knob::FilmGrain)].eligible==Level::Unchanged,
           "unverified film grain switch pretended to work");
    ensure(!p.settings[index(Knob::FoliageDensity)].accepted &&
           !p.settings[index(Knob::Reflections)].accepted,
           "render setting hook invented");
}
void ambient_density_mission_integrity_gates() {
    auto h=every_verified_hook();
    h.non_rt_renderer_complete=true;
    const auto p0=propose_1080p60(nvidia_1080p(),h);
    ensure(!p0.settings[index(Knob::NpcAmbientDensity)].accepted &&
           !p0.settings[index(Knob::TrafficAmbientDensity)].accepted,
           "NPC or traffic reduced without player opt in");
    Goals g{};
    g.allow_ambient_npc_reduction=true;
    g.allow_ambient_traffic_reduction=true;
    const auto p1=propose_1080p60(nvidia_1080p(),h,g);
    ensure(!p1.settings[index(Knob::NpcAmbientDensity)].accepted &&
           !p1.settings[index(Knob::TrafficAmbientDensity)].accepted,
           "mission-sensitive simulation altered without a safety gate");
    h.npc_changes_mission_safe=true;
    const auto p2=propose_1080p60(nvidia_1080p(),h,g);
    ensure(p2.settings[index(Knob::NpcAmbientDensity)].accepted &&
           !p2.settings[index(Knob::TrafficAmbientDensity)].accepted,
           "NPC and traffic mission safety accidentally conflated");
    h.traffic_changes_mission_safe=true;
    const auto p3=propose_1080p60(nvidia_1080p(),h,g);
    ensure(p3.settings[index(Knob::NpcAmbientDensity)].eligible==
           Level::Medium &&
           p3.settings[index(Knob::TrafficAmbientDensity)].eligible==
           Level::Medium,"mission-safe ambient settings not eligible");
}
void linux_vulkan_fsr_does_not_leak_fg() {
    Capabilities c{};
    c.os=Os::Linux;
    c.vendor=Vendor::Amd;
    c.vulkan_renderer_ready=true;
    c.temporal_input_ready=true;
    c.frame_generation_input_ready=true;
    c.fsr_sr_ready=true;
    c.fsr_fg_ready=true;
    c.io_uring_ready=true;
    c.vulkan.fsr_sr=true;
    c.vulkan.fsr_fg=true;
    const auto p=propose_1080p60(c,GameHooks{});
    ensure(p.pc.backend==Backend::Vulkan &&
           p.pc.upscaler==Upscaler::Fsr &&
           p.upscaling==UpscaleMode::Quality &&
           p.pc.frame_gen==FrameGen::Off &&
           p.pc.storage==Storage::IoUring,
           "Linux native profile invented D3D12 or FG");
}
void naming_and_no_performance_illusion() {
    const auto p=propose_1080p60(nvidia_1080p(),every_verified_hook());
    ensure(!p.fps_measured && !p.real_title_tested,
           "settings policy pretends game FPS was measured");
    ensure(std::string(name(Knob::ObjectDistance))=="object-distance" &&
           std::string(name(Knob::FilmGrain))=="film-grain" &&
           std::string(name(Level::Unchanged))=="unchanged" &&
           std::string(name(UpscaleMode::Quality))=="quality",
           "public settings names changed");
}
}
int main(){
    try{
        empty_support_never_claims_game_mods();
        verified_dlss_quality_and_reflex_without_fg();
        balanced_opt_in_and_no_unintegrated_dlss();
        ray_tracing_cannot_be_disabled_without_real_raster_path();
        real_settings_only_when_hook_is_verified();
        ambient_density_mission_integrity_gates();
        linux_vulkan_fsr_does_not_leak_fg();
        naming_and_no_performance_illusion();
    }catch(const std::exception& error){
        std::cerr<<"native 1080p quality target policy FAIL: "<<error.what()<<'\n';
        return 1;
    }
    std::cout<<"{\"kind\":\"native-1080p60-settings-contract\","
             "\"scenarios\":8,\"target_native_fps\":60,"
             "\"frame_generation_requested\":false,"
             "\"game_hardware_fps_measured\":false,"
             "\"game_hooks_actually_changed\":false}\n";
    return 0;
}
