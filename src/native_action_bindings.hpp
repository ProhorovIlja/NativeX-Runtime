// Original native PC keyboard/mouse/gamepad action remapping, no virtual devices.
// One owner thread for profile mutation; evaluate from immutable frame snapshots.
#pragma once
#include "native_input_core.hpp"
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
namespace scarlett::bindings {
enum class Source : std::uint8_t {
    KeyScan=1, MouseButton=2, GamepadButton=3,
    GamepadLeftTrigger=4, GamepadRightTrigger=5
};
struct Binding {
    Source source=Source::KeyScan;
    std::uint16_t code=0;           // scan 1..1023, mouse 0..4, gamepad button bit 0..15
    std::uint16_t modifier_scan=0;  // optional key 1..1023, 0=none
    std::uint8_t gamepad_slot=0;    // XInput-style 0..3, not persistent hardware ID
    std::uint8_t threshold=0;       // trigger 1..255, unused for button/key bindings
};
struct ActionState {
    std::bitset<32> pressed{};
    bool down(std::uint8_t action) const noexcept {
        return action<pressed.size() && pressed.test(action);
    }
};
class Map {
public:
    static constexpr std::size_t Actions=32;
    static constexpr std::size_t Alternatives=4;
    static constexpr std::size_t SerializedBytes=16u+Actions*Alternatives*8u+8u;
    bool bind(std::uint8_t action,std::uint8_t alternative,
              Binding binding) noexcept;
    bool clear(std::uint8_t action,std::uint8_t alternative) noexcept;
    bool get(std::uint8_t action,std::uint8_t alternative,Binding& out) const noexcept;
    ActionState evaluate(const input::FrameSnapshot& frame) const noexcept;
    // Portable, deterministic little-endian format, no process-specific device IDs.
    std::array<std::uint8_t,SerializedBytes> serialize() const noexcept;
    // Invalid input MUST NOT change the prior profile.
    static bool deserialize(const std::uint8_t* bytes,std::size_t length,
                            Map& destination) noexcept;
private:
    struct Slot {
        bool enabled=false;
        Binding binding{};
    };
    std::array<Slot,Actions*Alternatives> slots_{};
    static bool valid(Binding binding) noexcept;
    static bool activated(Binding binding,const input::FrameSnapshot& frame) noexcept;
};
} // namespace scarlett::bindings
