#include "native_input_bindings.hpp"
#include <cmath>

namespace scarlett::controls {
namespace {
constexpr std::size_t to_index(Action action) noexcept {
    return static_cast<std::size_t>(action);
}
bool valid_context(Context c) noexcept {
    return c==Context::Gameplay||c==Context::Menu;
}
bool valid_binding(const Binding& b) noexcept {
    if(!b.enabled)return true;
    if(!valid_context(b.context))return false;
    switch(b.source){
        case Source::ScanCode:
            return b.code>0 && b.code<input::Router::KeySlots;
        case Source::MouseButton:
            return b.code<5;
        case Source::GamepadButton:
            return b.code!=0 && (b.code&(b.code-1u))==0;
    }
    return false;
}
bool active_binding(const Binding& b,const input::FrameSnapshot& snapshot) noexcept {
    if(!b.enabled)return false;
    switch(b.source){
        case Source::ScanCode:
            return snapshot.key_down(b.code);
        case Source::MouseButton:
            return snapshot.button_down(static_cast<std::uint8_t>(b.code));
        case Source::GamepadButton:
            for(const auto& pad:snapshot.pads)
                if(pad.connected && (pad.buttons&b.code)!=0)return true;
            return false;
    }
    return false;
}
}
BindResult Mapper::bind(Action action,std::size_t slot,Binding value) noexcept {
    const auto target=to_index(action);
    if(target>=ActionCount)return BindResult::InvalidAction;
    if(slot>=SlotsPerAction)return BindResult::InvalidSlot;
    if(!valid_binding(value))return BindResult::InvalidInput;
    if(value.enabled) {
        for(std::size_t a=0;a<ActionCount;a++){
            for(std::size_t i=0;i<SlotsPerAction;i++){
                if(a==target && i==slot)continue;
                const auto& other=binds_[a][i];
                if(other.enabled && other.context==value.context &&
                   other.source==value.source && other.code==value.code)
                    return BindResult::Conflict;
            }
        }
    }
    binds_[target][slot]=value.enabled?value:Binding{};
    needs_rearm_=true; // held controls must release before reactivation
    return BindResult::Ok;
}
BindResult Mapper::clear(Action action,std::size_t slot) noexcept {
    return bind(action,slot,Binding{});
}
Binding Mapper::binding(Action action,std::size_t slot) const noexcept {
    const auto index=to_index(action);
    return index<ActionCount&&slot<SlotsPerAction?binds_[index][slot]:Binding{};
}
bool Mapper::set_look_settings(LookSettings settings) noexcept {
    if(!std::isfinite(settings.sensitivity) ||
       settings.sensitivity<0.05f || settings.sensitivity>10.0f)return false;
    look_=settings;
    return true;
}
bool Mapper::physical_down(std::size_t action,const input::FrameSnapshot& snapshot,
                           Context context) const noexcept {
    for(const auto& binding:binds_[action])
        if(binding.enabled && binding.context==context &&
           active_binding(binding,snapshot))return true;
    return false;
}
Frame Mapper::evaluate(const input::FrameSnapshot& snapshot,Context context) noexcept {
    Frame frame{};
    frame.context=context;
    frame.focused=snapshot.focused;
    if(context!=context_){
        context_=context;
        needs_rearm_=true; // no phantom MenuAccept when swapping contexts
    }
    if(!snapshot.focused){
        for(std::size_t a=0;a<ActionCount;a++)
            frame.actions[a].released=held_[a];
        reset();
        return frame;
    }
    const bool rearm=needs_rearm_;
    for(std::size_t a=0;a<ActionCount;a++){
        const bool physical=physical_down(a,snapshot,context);
        if(rearm)inhibited_[a]=physical;
        if(!physical)inhibited_[a]=false;
        const bool now=physical && !inhibited_[a];
        frame.actions[a]={now,!held_[a]&&now,held_[a]&&!now};
        held_[a]=now;
    }
    needs_rearm_=false;
    if(context==Context::Gameplay){
        frame.look_x=static_cast<float>(snapshot.motion.relative_x)*look_.sensitivity;
        const float y=static_cast<float>(snapshot.motion.relative_y)*look_.sensitivity;
        frame.look_y=look_.invert_y?-y:y;
        frame.wheel_vertical=snapshot.motion.wheel_vertical;
        frame.wheel_horizontal=snapshot.motion.wheel_horizontal;
        frame.raw_motion_saturated=snapshot.motion.saturated;
    }
    return frame;
}
void Mapper::reset() noexcept {
    held_.fill(false);
    inhibited_.fill(false);
    needs_rearm_=true;
}
Mapper Mapper::desktop_example() noexcept {
    Mapper result{};
    const auto key=[&](Action a,std::uint16_t code,Context context=Context::Gameplay,
                       std::size_t slot=0){
        (void)result.bind(a,slot,{Source::ScanCode,code,context,true});
    };
    const auto mouse=[&](Action a,std::uint16_t button){
        (void)result.bind(a,0,{Source::MouseButton,button,Context::Gameplay,true});
    };
    const auto pad=[&](Action a,std::uint16_t mask,Context context,
                     std::size_t slot){
        (void)result.bind(a,slot,{Source::GamepadButton,mask,context,true});
    };
    // Illustrative PC profile, NOT inferred GTA VI controls.
    key(Action::Forward,0x11);key(Action::Backward,0x1f);
    key(Action::Left,0x1e);key(Action::Right,0x20);
    key(Action::Sprint,0x2a);key(Action::Jump,0x39);
    key(Action::Interact,0x12);key(Action::Reload,0x13);
    key(Action::Pause,0x01);
    mouse(Action::PrimaryAttack,0);mouse(Action::Aim,1);
    pad(Action::Jump,0x1000,Context::Gameplay,1);
    pad(Action::Interact,0x4000,Context::Gameplay,1);
    pad(Action::PrimaryAttack,0x0200,Context::Gameplay,1);
    pad(Action::Pause,0x0010,Context::Gameplay,1);
    key(Action::MenuAccept,0x1c,Context::Menu);
    key(Action::MenuBack,0x01,Context::Menu);
    pad(Action::MenuAccept,0x1000,Context::Menu,1);
    pad(Action::MenuBack,0x2000,Context::Menu,1);
    return result;
}
const char* name(Action a) noexcept {
    switch(a) {
        case Action::Forward:return "forward";
        case Action::Backward:return "backward";
        case Action::Left:return "left";
        case Action::Right:return "right";
        case Action::Sprint:return "sprint";
        case Action::Jump:return "jump";
        case Action::Interact:return "interact";
        case Action::PrimaryAttack:return "primary-attack";
        case Action::Aim:return "aim";
        case Action::Reload:return "reload";
        case Action::Pause:return "pause";
        case Action::MenuAccept:return "menu-accept";
        case Action::MenuBack:return "menu-back";
        case Action::Count:return "invalid";
    }
    return "invalid";
}
const char* name(BindResult b) noexcept {
    switch(b) {
        case BindResult::Ok:return "ok";
        case BindResult::InvalidAction:return "invalid-action";
        case BindResult::InvalidSlot:return "invalid-slot";
        case BindResult::InvalidInput:return "invalid-input";
        case BindResult::Conflict:return "conflict";
    }
    return "invalid";
}
} // namespace scarlett::controls
