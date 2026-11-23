#include "native_1080p60_profile.hpp"

namespace scarlett::tuning {
namespace {
constexpr std::array<Level,KnobCount> Requested{{
    Level::Off,    // ray tracing (only if a full raster alternative exists)
    Level::Low,    // shadow resolution/filter/distance
    Level::Low,    // volumetric fog/clouds
    Level::Medium, // raster / screen-space reflections where available
    Level::Medium, // foliage density
    Level::Medium, // foliage render distance
    Level::Medium, // noncritical object draw distance
    Level::Medium, // ambient NPC density; does not alter missions by default
    Level::Medium, // ambient traffic density; does not alter missions by default
    Level::Off,    // chromatic aberration
    Level::Off,    // film grain
    Level::Off,    // motion blur
    Level::Off     // lens distortion
}};
constexpr std::size_t at(Knob k) noexcept {
    return static_cast<std::size_t>(k);
}
}

Plan propose_1080p60(const Capabilities& capability,const GameHooks& hooks,
                     const Goals& goals) noexcept {
    Plan plan{};
    Preferences requested{};
    requested.backend=Backend::Auto;
    requested.upscaler=Upscaler::Auto;
    requested.frame_gen=FrameGen::Off; // user insists on real, not generated frames
    requested.latency=Latency::Auto;
    requested.storage=Storage::Auto;
    requested.neural=Neural::Off; // DLSS5 neural rendering is not Super Resolution
    // Existing resolver checks integration with the selected API and complete
    // temporal inputs, not a loose file/module-presence flag.
    plan.pc=resolve(requested,capability);
    if(plan.pc.renderer_usable && plan.pc.upscaler!=Upscaler::Native)
        plan.upscaling=goals.prefer_balanced_upscaling?
            UpscaleMode::Balanced:UpscaleMode::Quality;

    for(std::size_t i=0;i<KnobCount;i++){
        auto& decision=plan.settings[i];
        decision.requested=Requested[i];
        decision.hook_verified=hooks.verified[i];
        bool safe=decision.hook_verified;
        if(i==at(Knob::RayTracing))
            safe=safe && hooks.non_rt_renderer_complete;
        if(i==at(Knob::NpcAmbientDensity))
            safe=safe && goals.allow_ambient_npc_reduction &&
                 hooks.npc_changes_mission_safe;
        if(i==at(Knob::TrafficAmbientDensity))
            safe=safe && goals.allow_ambient_traffic_reduction &&
                 hooks.traffic_changes_mission_safe;
        // "eligible" only means the profile may request this change from a
        // real implemented hook; the profile never applies hooks by itself.
        if(safe){
            decision.eligible=decision.requested;
            decision.accepted=true;
        }
    }
    plan.raster_only_eligible=
        plan.settings[at(Knob::RayTracing)].accepted;
    return plan;
}
const char* name(Knob v) noexcept {
    switch(v){
        case Knob::RayTracing:return "ray-tracing";
        case Knob::Shadows:return "shadows";
        case Knob::Volumetrics:return "volumetrics";
        case Knob::Reflections:return "raster-reflections";
        case Knob::FoliageDensity:return "foliage-density";
        case Knob::FoliageDistance:return "foliage-distance";
        case Knob::ObjectDistance:return "object-distance";
        case Knob::NpcAmbientDensity:return "ambient-npc-density";
        case Knob::TrafficAmbientDensity:return "ambient-traffic-density";
        case Knob::ChromaticAberration:return "chromatic-aberration";
        case Knob::FilmGrain:return "film-grain";
        case Knob::MotionBlur:return "motion-blur";
        case Knob::LensDistortion:return "lens-distortion";
        default:return "unknown";
    }
}
const char* name(Level v) noexcept {
    switch(v){
        case Level::Unchanged:return "unchanged";
        case Level::Off:return "off";
        case Level::Low:return "low";
        case Level::Medium:return "medium";
        case Level::High:return "high";
    }
    return "unknown";
}
const char* name(UpscaleMode v) noexcept {
    switch(v){
        case UpscaleMode::Native:return "native";
        case UpscaleMode::Quality:return "quality";
        case UpscaleMode::Balanced:return "balanced";
    }
    return "unknown";
}
} // namespace scarlett::tuning
