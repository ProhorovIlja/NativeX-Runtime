#include "native_input_core.hpp"

namespace scarlett::input {
namespace {
bool gamepads_equal(const Gamepad& a,const Gamepad& b) {
    return a.connected==b.connected && a.buttons==b.buttons &&
        a.left_x==b.left_x && a.left_y==b.left_y &&
        a.right_x==b.right_x && a.right_y==b.right_y &&
        a.left_trigger==b.left_trigger && a.right_trigger==b.right_trigger;
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
    slot->keys.set(index,pressed); // Per-device typematic suppression.
    Event e{};e.type=Type::Key;e.clock_ticks=ticks;e.device=device;
    e.code=index;e.pressed=pressed;emit(e);
}
void Router::mouse_move(std::uint64_t device,std::int32_t x,std::int32_t y,
                        bool absolute,std::uint64_t ticks) {
    if(!focused_ || (!absolute && x==0 && y==0))return;
    Event e{};e.type=absolute?Type::MouseAbsolute:Type::MouseRelative;
    e.device=device;e.clock_ticks=ticks;e.x=x;e.y=y;emit(e);
}
void Router::mouse_button(std::uint64_t device,std::uint8_t button,bool pressed,
                          std::uint64_t ticks) {
    if(!focused_ || button>=5)return;
    auto* slot=ensure_device(device);
    if(!slot || slot->buttons.test(button)==pressed)return;
    slot->buttons.set(button,pressed);
    Event e{};e.type=Type::MouseButton;e.device=device;e.code=button;
    e.pressed=pressed;e.clock_ticks=ticks;emit(e);
}
void Router::mouse_wheel(std::uint64_t device,std::int32_t delta,bool horizontal,
                         std::uint64_t ticks) {
    if(!focused_ || delta==0)return;
    Event e{};e.type=horizontal?Type::WheelHorizontal:Type::WheelVertical;
    e.device=device;e.y=delta;e.clock_ticks=ticks;emit(e);
}
void Router::raw_device_removed(std::uint64_t id,std::uint64_t ticks) {
    auto* slot=lookup(id);
    if(!slot)return;
    *slot=RawDevice{}; // Atomically discard all held controls from this device.
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
    pads_[slot]=value;
    Event e{};e.type=Type::Gamepad;e.code=slot;e.gamepad=value;
    e.clock_ticks=ticks;emit(e);
}
void Router::set_focus(bool focused,std::uint64_t ticks) {
    if(focused_==focused)return;
    focused_=focused;
    if(!focused) {
        devices_.fill({});
        pads_.fill({});
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
} // namespace scarlett::input
