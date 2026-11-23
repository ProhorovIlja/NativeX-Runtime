// Original source-only native Windows/Linux graphics and scene tuning policy.
// A declared hook is not a GTA VI hook and never changes the game by itself.
#pragma once
#include "pc_feature_policy.hpp"
#include <array>
#include <cstddef>

namespace scarlett::tuning {
enum class Knob : std::size_t {
    RayTracing, Shadows, Volumetrics, Reflections,
    FoliageDensity, FoliageDistance, ObjectDistance,
    NpcAmbientDensity, TrafficAmbientDensity,
    ChromaticAberration, FilmGrain, MotionBlur, LensDistortion,
    Count
};
constexpr std::size_t KnobCount=static_cast<std::size_t>(Knob::Count);
enum class Level { Unchanged, Off, Low, Medium, High };
enum class UpscaleMode { Native, Quality, Balanced };

// All false by default. Verification requires the real game's rendering and
// simulation hooks; CLI flags, DLL presence or an assumed game setting do not
// establish these capabilities.
struct GameHooks {
    std::array<bool,KnobCount> verified{};
    bool non_rt_renderer_complete=false; // turning off RT must not black out game
    bool npc_changes_mission_safe=false; // ambient changes preserve scripted actors
    bool traffic_changes_mission_safe=false; // preserve mission traffic/routes
};
struct Goals {
    UpscaleMode preferred_upscaling=UpscaleMode::Quality; // Native / Quality / Balanced
    bool allow_ambient_npc_reduction=false;
    bool allow_ambient_traffic_reduction=false;
};
struct SettingDecision {
    Level requested=Level::Unchanged;
    Level eligible=Level::Unchanged; // accepted by policy, NOT applied in a game
    bool hook_verified=false;
    bool accepted=false;
};
struct Plan {
    int output_width=1920;
    int output_height=1080;
    int target_native_fps=60; // acceptance objective, never an estimated FPS
    bool fps_measured=false;
    bool real_title_tested=false;
    bool raster_only_eligible=false; // only with verified independent raster path
    UpscaleMode upscaling=UpscaleMode::Native;
    Selection pc{};
    std::array<SettingDecision,KnobCount> settings{};
};
Plan propose_1080p60(const Capabilities& pc,const GameHooks& hooks,
                     const Goals& goals={});
const char* name(Knob knob) noexcept;
const char* name(Level level) noexcept;
const char* name(UpscaleMode mode) noexcept;
} // namespace scarlett::tuning
