#include "native_input_core.hpp"
#include <limits>

namespace scarlett::input {
namespace {
bool gamepads_equal(const Gamepad& a,const Gamepad& b) {
    return a.connected==b.connected && a.buttons==b.buttons &&
        a.left_x==b.left_x && a.left_y==b.left_y &&
        a.right_x==b.right_x && a.right_y==b.right_y &&
        a.left_trigger==b.left_trigger && a.right_trigger==b.right_trigger;
}
std::int32_t add_motion(std::int32_t current,std::int32_t delta,
                        bool& saturated) noexcept {
    const std::int64_t sum=static_cast<std::int64_t>(current)+delta;
    if(sum>std::numeric_limits<std::int32_t>::max()){
        saturated=true;
        return std::numeric_limits<std::int32_t>::max();
    }
    if(sum<std::numeric_limits<std::int32_t>::min()){
        saturated=true;
        return std::numeric_limits<std::int32_t>::min();
    }
    return static_cast<std::int32_t>(sum);
}
void note_motion(MouseMotion& motion) noexcept {
    if(motion.packets==std::numeric_limits<std::uint64_t>::max())
        motion.saturated=true;
    else
        ++motion.packets;
}
}
Router::RawDevice* Router::lookup(std::uint64_t id) {
    for(auto& slot:devices_) if(slot.active && slot.id==id)return &slot;
    return nullptr;
}
const Router::RawDevice* Router::lookup(std::uint64_t id) const {
    for(const auto& slot:devices_)if(slot.active && slot.id==id)return &slot;
    return nullptr;
}
Router::RawDevice* Router::ensure_device(std::uint64_t id) {
    if(auto* existing=lookup(id))return existing;
    for(auto& slot:devices_) {
        if(!slot.active) {
            slot=RawDevice{};
            slot.active=true;slot.id=id;
            return &slot;
        }
    }
    // Fail closed under an unexpectedly large set of distinct Raw Input devices.
    ++dropped_;
    return nullptr;
}
void Router::emit(const Event& event) {
    if (count_==QueueLimit) {++dropped_;return;}
    events_[tail_]=event;
    tail_=(tail_+1u)%QueueLimit;
    ++count_;
}
void Router::key(std::uint64_t device,std::uint16_t scan,bool e0,bool e1,
                 bool pressed,std::uint64_t ticks) {
    if(!focused_ || scan==0 || scan>255)return;
    const auto index=static_cast<std::uint16_t>(scan | (e0?0x100u:0u) |
                                                 (e1?0x200u:0u));
    auto* slot=ensure_device(device);
    if(!slot || slot->keys.test(index)==pressed)return;
    const bool was_down=key_down(index);
    slot->keys.set(index,pressed); // Per-device typematic suppression.
    const bool now_down=key_down(index);
    if(was_down!=now_down) {
        (now_down?key_press_edges_:key_release_edges_).set(index);
    }
    Event e{};e.type=Type::Key;e.clock_ticks=ticks;e.device=device;
    e.code=index;e.pressed=pressed;emit(e);
}
void Router::mouse_move(std::uint64_t device,std::int32_t x,std::int32_t y,
                        bool absolute,std::uint64_t ticks) {
    if(!focused_ || (!absolute && x==0 && y==0))return;
    // Keep the camera-useful raw relative displacement even if the separate
    // diagnostic event ring is full. Never confuse absolute coordinates with
    // relative movement or turn the mouse into an emulated analog stick.
    if(!absolute) {
        mouse_motion_.relative_x=add_motion(mouse_motion_.relative_x,x,
                                            mouse_motion_.saturated);
        mouse_motion_.relative_y=add_motion(mouse_motion_.relative_y,y,
                                            mouse_motion_.saturated);
        note_motion(mouse_motion_);
    }
    Event e{};e.type=absolute?Type::MouseAbsolute:Type::MouseRelative;
    e.device=device;e.clock_ticks=ticks;e.x=x;e.y=y;emit(e);
}
void Router::mouse_button(std::uint64_t device,std::uint8_t button,bool pressed,
                          std::uint64_t ticks) {
    if(!focused_ || button>=5)return;
    auto* slot=ensure_device(device);
    if(!slot || slot->buttons.test(button)==pressed)return;
    const bool was_down=button_down(button);
    slot->buttons.set(button,pressed);
    const bool now_down=button_down(button);
    if(was_down!=now_down) {
        (now_down?mouse_press_edges_:mouse_release_edges_).set(button);
    }
    Event e{};e.type=Type::MouseButton;e.device=device;e.code=button;
    e.pressed=pressed;e.clock_ticks=ticks;emit(e);
}
void Router::mouse_wheel(std::uint64_t device,std::int32_t delta,bool horizontal,
                         std::uint64_t ticks) {
    if(!focused_ || delta==0)return;
    auto& total=horizontal?mouse_motion_.wheel_horizontal
                          :mouse_motion_.wheel_vertical;
    total=add_motion(total,delta,mouse_motion_.saturated);
    note_motion(mouse_motion_);
    Event e{};e.type=horizontal?Type::WheelHorizontal:Type::WheelVertical;
    e.device=device;e.y=delta;e.clock_ticks=ticks;emit(e);
}
void Router::raw_device_removed(std::uint64_t id,std::uint64_t ticks) {
    auto* slot=lookup(id);
    if(!slot)return;
    const auto old_keys=slot->keys;
    const auto old_buttons=slot->buttons;
    *slot=RawDevice{}; // Atomically discard all held controls from this device.
    for(std::size_t key=0;key<KeySlots;key++) {
        if(old_keys.test(key) && !key_down(static_cast<std::uint16_t>(key)))
            key_release_edges_.set(key);
    }
    for(std::uint8_t button=0;button<5;button++) {
        if(old_buttons.test(button) && !button_down(button))
            mouse_release_edges_.set(button);
    }
    // Guarantee the consumer is notified even if the event ring is full.
    // It must resynchronize from current state when seeing this notification.
    if(count_==QueueLimit) {
        dropped_+=count_;
        count_=0;head_=0;tail_=0;
    }
    Event e{};e.type=Type::DeviceDisconnected;e.device=id;e.clock_ticks=ticks;
    emit(e);
}
void Router::gamepad(std::uint8_t slot,const Gamepad& value,std::uint64_t ticks) {
    if(!focused_ || slot>=pads_.size())return;
    if(gamepads_equal(pads_[slot],value))return;
    const auto old_buttons=combined_gamepad_buttons();
    pads_[slot]=value;
    const auto new_buttons=combined_gamepad_buttons();
    gamepad_press_edges_|=static_cast<std::uint16_t>(new_buttons &
        static_cast<std::uint16_t>(~old_buttons));
    gamepad_release_edges_|=static_cast<std::uint16_t>(old_buttons &
        static_cast<std::uint16_t>(~new_buttons));
    Event e{};e.type=Type::Gamepad;e.code=slot;e.gamepad=value;
    e.clock_ticks=ticks;emit(e);
}
void Router::set_focus(bool focused,std::uint64_t ticks) {
    if(focused_==focused)return;
    focused_=focused;
    if(!focused) {
        devices_.fill({});
        pads_.fill({});
        mouse_motion_={};  // No stale camera movement after Alt+Tab.
        key_press_edges_.reset();key_release_edges_.reset();
        mouse_press_edges_.reset();mouse_release_edges_.reset();
        gamepad_press_edges_=0;gamepad_release_edges_=0;
        // Invalidate stale messages; guarantee one focus-lost notification.
        head_=0;tail_=0;count_=0;
    }
    Event e{};e.type=focused?Type::FocusGained:Type::FocusLost;
    e.clock_ticks=ticks;emit(e);
}
bool Router::pop(Event& out) {
    if(count_==0)return false;
    out=events_[head_];head_=(head_+1u)%QueueLimit;--count_;
    return true;
}
bool Router::key_down(std::uint16_t scan) const {
    if(scan>=KeySlots)return false;
    for(const auto& d:devices_)if(d.active && d.keys.test(scan))return true;
    return false;
}
bool Router::button_down(std::uint8_t button) const {
    if(button>=5)return false;
    for(const auto& d:devices_)if(d.active && d.buttons.test(button))return true;
    return false;
}
bool Router::device_key_down(std::uint64_t id,std::uint16_t scan) const {
    const auto* d=lookup(id);
    return d && scan<KeySlots && d->keys.test(scan);
}
bool Router::device_button_down(std::uint64_t id,std::uint8_t button) const {
    const auto* d=lookup(id);
    return d && button<5 && d->buttons.test(button);
}
Gamepad Router::gamepad_state(std::uint8_t slot) const {
    return slot<pads_.size()?pads_[slot]:Gamepad{};
}
std::uint16_t Router::combined_gamepad_buttons() const noexcept {
    std::uint16_t buttons=0;
    for(const auto& pad:pads_)if(pad.connected)buttons|=pad.buttons;
    return buttons;
}
FrameSnapshot Router::snapshot(std::uint64_t receipt_ticks) const noexcept {
    FrameSnapshot result{};
    result.captured_ticks=receipt_ticks;
    result.focused=focused_;
    result.dropped_events=dropped_;
    result.pads=pads_;
    result.motion=mouse_motion_;
    result.key_press_edges=key_press_edges_;
    result.key_release_edges=key_release_edges_;
    result.mouse_press_edges=mouse_press_edges_;
    result.mouse_release_edges=mouse_release_edges_;
    result.gamepad_press_edges=gamepad_press_edges_;
    result.gamepad_release_edges=gamepad_release_edges_;
    for(const auto& d:devices_) {
        if(!d.active)continue;
        result.keys|=d.keys;
        result.mouse_buttons|=d.buttons;
    }
    return result;
}
FrameSnapshot Router::capture_frame(std::uint64_t receipt_ticks) noexcept {
    const auto frame=snapshot(receipt_ticks);
    // Raw events still remain available for diagnostics. Independent edges
    // survive event-ring overflow, but are delivered only once per frame.
    mouse_motion_={};
    key_press_edges_.reset();key_release_edges_.reset();
    mouse_press_edges_.reset();mouse_release_edges_.reset();
    gamepad_press_edges_=0;gamepad_release_edges_=0;
    return frame;
}
} // namespace scarlett::input
