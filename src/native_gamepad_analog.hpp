// Original platform-independent native gamepad analog processing.
// Per-frame normalized input, no virtual controller, no smoothing/added latency.
#pragma once
#include "native_input_core.hpp"
#include <cstddef>
#include <cstdint>
namespace scarlett::analog {
enum class Curve : std::uint8_t { Linear=0, SmoothCubic=1 };
struct StickConfig {
    // Native signed stick range is [-32768,32767]. All thresholds are raw units.
    std::uint16_t inner_deadzone=7000;
    std::uint16_t outer_deadzone=1500;
    Curve curve=Curve::Linear;
    bool invert_x=false;
    bool invert_y=false;
};
struct Stick {
    float x=0.0f;
    float y=0.0f;
};
struct Controller {
    bool connected=false;
    Stick left{},right{};
    float left_trigger=0.0f,right_trigger=0.0f;
};
enum class Status : std::uint8_t { Ok, InvalidSlot, InvalidConfig };
// A rejected configuration MUST leave output unchanged. An unfocused or
// disconnected frame is a valid zero/neutral output; no historical state.
Status sample(const input::FrameSnapshot& frame,std::size_t slot,
              const StickConfig& left,const StickConfig& right,
              Controller& output) noexcept;
const char* status_name(Status status) noexcept;
} // namespace scarlett::analog
