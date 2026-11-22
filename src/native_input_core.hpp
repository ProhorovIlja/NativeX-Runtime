// Original platform-independent, per-physical-device PC input state core.
// No controller emulation, global hooks, game binary or vendor SDK.
#pragma once
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>

namespace scarlett::input {
enum class Type : std::uint8_t {
    Key, MouseRelative, MouseAbsolute, MouseButton, WheelVertical,
    WheelHorizontal, Gamepad, FocusLost, FocusGained, DeviceDisconnected
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
// Immutable value returned by Router::snapshot() on the single input owner
// thread. Copy into the simulation frame; never use raw device IDs in a save.
struct FrameSnapshot {
    std::uint64_t captured_ticks=0;
    std::bitset<1024> keys{};
    std::bitset<5> mouse_buttons{};
    std::array<Gamepad,4> pads{};
    std::uint64_t dropped_events=0;
    bool focused=true;
    bool key_down(std::uint16_t scan) const noexcept {
        return scan<keys.size() && keys.test(scan);
    }
    bool button_down(std::uint8_t button) const noexcept {
        return button<mouse_buttons.size() && mouse_buttons.test(button);
    }
};
struct Event {
    Type type = Type::Key;
    std::uint64_t clock_ticks = 0; // platform monotonic receipt ticks, not hardware timestamp
    std::uint64_t device = 0; // ephemeral process-local token, never persist handle
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
    static constexpr std::size_t RawDeviceSlots = 16;
    // All APIs are for one owner/polling thread, NOT concurrent use without a lock.
    void key(std::uint64_t device,std::uint16_t scan_code,bool e0,bool e1,
             bool pressed,std::uint64_t ticks);
    void mouse_move(std::uint64_t device,std::int32_t x,std::int32_t y,
                    bool absolute,std::uint64_t ticks);
    void mouse_button(std::uint64_t device,std::uint8_t button,bool pressed,
                      std::uint64_t ticks);
    void mouse_wheel(std::uint64_t device,std::int32_t delta,bool horizontal,
                     std::uint64_t ticks);
    void raw_device_removed(std::uint64_t device,std::uint64_t ticks);
    void gamepad(std::uint8_t slot,const Gamepad& value,std::uint64_t ticks);
    void set_focus(bool focused,std::uint64_t ticks);
    bool pop(Event& out);
    // Aggregate state is the OR of active physical device states, avoiding
    // premature release if two keyboards/mice press the same control.
    bool key_down(std::uint16_t scan_code) const;
    bool button_down(std::uint8_t button) const;
    bool device_key_down(std::uint64_t device,std::uint16_t scan_code) const;
    bool device_button_down(std::uint64_t device,std::uint8_t button) const;
    Gamepad gamepad_state(std::uint8_t slot) const;
    FrameSnapshot snapshot(std::uint64_t receipt_ticks) const noexcept;
    std::size_t queued() const {return count_;}
    std::uint64_t dropped() const {return dropped_;}
    bool focused() const {return focused_;}
private:
    struct RawDevice {
        bool active=false;
        std::uint64_t id=0;
        std::bitset<KeySlots> keys{};
        std::bitset<5> buttons{};
    };
    RawDevice* lookup(std::uint64_t id);
    const RawDevice* lookup(std::uint64_t id) const;
    RawDevice* ensure_device(std::uint64_t id);
    void emit(const Event& event);
    std::array<RawDevice,RawDeviceSlots> devices_{};
    std::array<Gamepad,GamepadSlots> pads_{};
    std::array<Event,QueueLimit> events_{};
    std::size_t head_=0,tail_=0,count_=0;
    std::uint64_t dropped_=0; // events dropped AND raw-device table capacity exhaustion
    bool focused_=true;
};
} // namespace scarlett::input
