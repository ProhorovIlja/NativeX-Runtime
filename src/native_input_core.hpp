// Original platform-independent event and state core for native PC input.
// No controller emulation, Windows hook, game binary, or vendor SDK.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace scarlett::input {
enum class Type : std::uint8_t {
    Key, MouseRelative, MouseAbsolute, MouseButton, WheelVertical,
    WheelHorizontal, Gamepad, FocusLost, FocusGained
};
struct Gamepad {
    bool connected = false;
    std::uint16_t buttons = 0;
    std::int16_t left_x = 0;
    std::int16_t left_y = 0;
    std::int16_t right_x = 0;
    std::int16_t right_y = 0;
    std::uint8_t left_trigger = 0;
    std::uint8_t right_trigger = 0;
};
struct Event {
    Type type = Type::Key;
    std::uint64_t clock_ticks = 0; // platform monotonic counter ticks, not claimed hardware timestamp
    std::uint64_t device = 0; // process-local token; do not persist device handle
    std::uint16_t code = 0;
    std::int32_t x = 0;
    std::int32_t y = 0;
    bool pressed = false;
    Gamepad gamepad{};
};
class Router {
public:
    static constexpr std::size_t KeySlots = 1024;
    static constexpr std::size_t QueueLimit = 512;
    static constexpr std::size_t GamepadSlots = 4;
    // The owner/polling thread serializes writes and reads. No locks on the input hot path.
    void key(std::uint64_t device,std::uint16_t scan_code,bool e0,bool e1,
             bool pressed,std::uint64_t ticks);
    void mouse_move(std::uint64_t device,std::int32_t x,std::int32_t y,
                    bool absolute,std::uint64_t ticks);
    void mouse_button(std::uint64_t device,std::uint8_t button,bool pressed,
                      std::uint64_t ticks);
    void mouse_wheel(std::uint64_t device,std::int32_t delta,bool horizontal,
                     std::uint64_t ticks);
    void gamepad(std::uint8_t slot,const Gamepad& value,std::uint64_t ticks);
    void set_focus(bool focused,std::uint64_t ticks);
    bool pop(Event& out);
    bool key_down(std::uint16_t scan_code) const;
    bool button_down(std::uint8_t button) const;
    Gamepad gamepad_state(std::uint8_t slot) const;
    std::size_t queued() const {return count_;}
    std::uint64_t dropped() const {return dropped_;}
    bool focused() const {return focused_;}
private:
    void emit(const Event& event);
    std::array<bool,KeySlots> keys_{};
    std::array<bool,5> buttons_{};
    std::array<Gamepad,GamepadSlots> pads_{};
    std::array<Event,QueueLimit> events_{};
    std::size_t head_=0,tail_=0,count_=0;
    std::uint64_t dropped_=0;
    bool focused_=true;
};
} // namespace scarlett::input
