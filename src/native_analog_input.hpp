// Native physical gamepad analog preprocessing for Windows and Linux.
// No controller emulation, game integration, OS SDK dependency or heap allocation.
#pragma once
#include "native_input_core.hpp"
#include <cstdint>

namespace scarlett::analog {
struct Config {
    float inner_deadzone = 0.12f;
    float outer_deadzone = 0.03f;
    float cubic_blend = 0.0f; // 0: linear response, 1: cubic response.
    std::uint8_t trigger_floor = 0;
    bool invert_left_y = false;
    bool invert_right_y = false;
};
struct Stick {
    float x = 0.0f;
    float y = 0.0f;
};
struct Frame {
    Stick left{};
    Stick right{};
    float left_trigger = 0.0f;
    float right_trigger = 0.0f;
};
// Explicitly reject NaN, infinity and impossible deadzone geometry.
bool valid(const Config& settings) noexcept;
// Read only from an immutable frame snapshot. Invalid settings, lost focus,
// disconnected pads or missing slots produce a neutral zero state.
Frame sample(const input::FrameSnapshot& snapshot,
             std::uint8_t pad_slot, const Config& settings) noexcept;
} // namespace scarlett::analog
