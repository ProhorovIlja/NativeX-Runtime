// Portable native keyboard, mouse and gamepad action binding policy.
// Source-built fixed-size model; not a hook into any GTA VI executable.
#pragma once
#include "native_input_core.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace scarlett::controls {
enum class Context : std::uint8_t { Gameplay, Menu };
enum class Action : std::uint8_t {
    Forward, Backward, Left, Right, Sprint, Jump, Interact,
    PrimaryAttack, Aim, Reload, Pause, MenuAccept, MenuBack, Count
};
enum class Source : std::uint8_t { ScanCode, MouseButton, GamepadButton };
enum class BindResult : std::uint8_t {
    Ok, InvalidAction, InvalidSlot, InvalidInput, Conflict
};
constexpr std::size_t ActionCount=static_cast<std::size_t>(Action::Count);
struct Binding {
    Source source=Source::ScanCode;
    std::uint16_t code=0;           // Win32 Raw Input scan+E0/E1, mouse 0..4, pad one bit
    Context context=Context::Gameplay;
    bool enabled=false;
};
struct ActionState {
    bool down=false;
    bool pressed=false;
    bool released=false;
};
struct LookSettings {
    float sensitivity=1.0f;         // scales raw integer mouse displacement
    bool invert_y=false;
};
struct Frame {
    std::array<ActionState,ActionCount> actions{};
    float look_x=0.0f;
    float look_y=0.0f;
    std::int32_t wheel_vertical=0;
    std::int32_t wheel_horizontal=0;
    bool raw_motion_saturated=false;
    bool focused=false;
    Context context=Context::Gameplay;
};
// One owner thread: ingest an input::Router::capture_frame snapshot after
// consuming OS input on each simulation frame. No background threads,
// virtual gamepad, raw event capture, polling or global input hooks.
class Mapper {
public:
    static constexpr std::size_t SlotsPerAction=2;
    BindResult bind(Action action,std::size_t slot,Binding binding) noexcept;
    BindResult clear(Action action,std::size_t slot) noexcept;
    Binding binding(Action action,std::size_t slot) const noexcept;
    bool set_look_settings(LookSettings settings) noexcept;
    LookSettings look_settings() const noexcept {return look_;}
    Frame evaluate(const input::FrameSnapshot& snapshot,Context context) noexcept;
    void reset() noexcept;
    static Mapper desktop_example() noexcept;
private:
    bool physical_down(std::size_t action,const input::FrameSnapshot& frame,
                       Context context) const noexcept;
    std::array<std::array<Binding,SlotsPerAction>,ActionCount> binds_{};
    std::array<bool,ActionCount> held_{};
    std::array<bool,ActionCount> inhibited_{};
    LookSettings look_{};
    Context context_=Context::Gameplay;
    bool needs_rearm_=true;
};
const char* name(Action action) noexcept;
const char* name(BindResult result) noexcept;
} // namespace scarlett::controls
