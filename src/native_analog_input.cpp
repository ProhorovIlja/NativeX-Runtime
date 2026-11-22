#include "native_analog_input.hpp"
#include <cmath>

namespace scarlett::analog {
namespace {
float clamp_unit(float value) noexcept {
    return value <= 0.0f ? 0.0f : (value >= 1.0f ? 1.0f : value);
}
float scale_axis(std::int16_t raw) noexcept {
    // Preserve both signed full-scale endpoints without int16 overflow.
    return raw < 0 ? static_cast<float>(raw) / 32768.0f
                   : static_cast<float>(raw) / 32767.0f;
}
Stick normalize_stick(std::int16_t raw_x, std::int16_t raw_y,
                      const Config& cfg, bool invert_y) noexcept {
    const float x = scale_axis(raw_x);
    const float y = invert_y ? -scale_axis(raw_y) : scale_axis(raw_y);
    const float length = std::sqrt(x*x + y*y);
    if (length <= cfg.inner_deadzone || length == 0.0f) return {};
    const float useful_range = 1.0f - cfg.inner_deadzone - cfg.outer_deadzone;
    const float linear = clamp_unit((length - cfg.inner_deadzone) / useful_range);
    const float shaped = linear + cfg.cubic_blend * (linear*linear*linear - linear);
    const float gain = shaped / length;
    return {x*gain, y*gain};
}
float normalize_trigger(std::uint8_t raw, std::uint8_t floor) noexcept {
    if (raw <= floor) return 0.0f;
    if (floor == 255) return 0.0f;
    return static_cast<float>(static_cast<unsigned>(raw) - floor) /
           static_cast<float>(255u - floor);
}
} // namespace
bool valid(const Config& cfg) noexcept {
    return std::isfinite(cfg.inner_deadzone) &&
           std::isfinite(cfg.outer_deadzone) &&
           std::isfinite(cfg.cubic_blend) &&
           cfg.inner_deadzone >= 0.0f && cfg.inner_deadzone < 1.0f &&
           cfg.outer_deadzone >= 0.0f && cfg.outer_deadzone < 1.0f &&
           cfg.inner_deadzone + cfg.outer_deadzone < 1.0f &&
           cfg.cubic_blend >= 0.0f && cfg.cubic_blend <= 1.0f;
}
Frame sample(const input::FrameSnapshot& snapshot,
             std::uint8_t slot, const Config& cfg) noexcept {
    if (!valid(cfg) || !snapshot.focused || slot >= snapshot.pads.size()) return {};
    const auto& pad = snapshot.pads[slot];
    if (!pad.connected) return {};
    Frame output{};
    output.left = normalize_stick(pad.left_x, pad.left_y, cfg, cfg.invert_left_y);
    output.right = normalize_stick(pad.right_x, pad.right_y, cfg, cfg.invert_right_y);
    output.left_trigger = normalize_trigger(pad.left_trigger, cfg.trigger_floor);
    output.right_trigger = normalize_trigger(pad.right_trigger, cfg.trigger_floor);
    return output;
}
} // namespace scarlett::analog
