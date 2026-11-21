#include "native_input_core.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace scarlett::input;
namespace {
void check(bool ok,const char* why) {if(!ok)throw std::runtime_error(why);}
Event next(Router& router) {
    Event value{};
    check(router.pop(value),"missing expected input event");
    return value;
}
void keys_extended_and_repeat() {
    Router r;
    r.key(11,0x1d,false,false,true,100);
    r.key(11,0x1d,false,false,true,101); // Typematic repetition is not a second keydown.
    r.key(11,0x1d,true,false,true,102);
    check(r.queued()==2 && r.key_down(0x1d) && r.key_down(0x11d),"extended keys collided");
    const auto first=next(r), second=next(r);
    check(first.type==Type::Key && first.code==0x1d &&
          first.device==11 && first.clock_ticks==100 && first.pressed,"key event metadata");
    check(second.code==0x11d && second.clock_ticks==102 && second.pressed,"E0 key");
    r.key(11,0x1d,false,false,false,103);
    check(!r.key_down(0x1d) && r.key_down(0x11d) && !next(r).pressed,"key release");
    r.key(11,0x1d,true,false,false,104);
    check(!r.key_down(0x11d),"E0 release");
}
void e1_and_invalid() {
    Router r;
    r.key(1,0x45,false,true,true,1);
    check(r.key_down(0x245) && next(r).code==0x245,"E1 key identity");
    r.key(1,0,false,false,true,2);
    r.key(1,256,false,false,true,3);
    check(r.queued()==0,"invalid make codes accepted");
    check(!r.key_down(1024),"out-of-bounds key query");
}
void mouse_modes_buttons_and_wheel() {
    Router r;
    r.mouse_move(10,17,-5,false,1);
    r.mouse_move(10,0,0,false,2);
    r.mouse_move(10,32000,12000,true,3);
    r.mouse_button(10,0,true,4);
    r.mouse_button(10,0,true,5);
    r.mouse_button(10,4,true,6);
    r.mouse_button(10,5,true,7); // invalid button
    r.mouse_wheel(10,120,false,8);
    r.mouse_wheel(10,-120,true,9);
    check(r.queued()==6 && r.button_down(0) && r.button_down(4),"mouse dedup or buttons");
    auto a=next(r),b=next(r);
    check(a.type==Type::MouseRelative && a.x==17 && a.y==-5,"relative mouse");
    check(b.type==Type::MouseAbsolute && b.x==32000 && b.y==12000,"absolute mode");
    check(next(r).type==Type::MouseButton && next(r).code==4,"mouse buttons");
    a=next(r);b=next(r);
    check(a.type==Type::WheelVertical && a.y==120 &&
          b.type==Type::WheelHorizontal && b.y==-120,"wheel sign/axis");
}
void gamepad_changes() {
    Router r;
    Gamepad pad{};pad.connected=true;pad.buttons=0x1000;
    pad.left_x=-32768;pad.right_trigger=255;
    r.gamepad(0,pad,42);r.gamepad(0,pad,43);
    check(r.queued()==1 && r.gamepad_state(0).left_x==-32768,"gamepad update/dedup");
    const auto e=next(r);
    check(e.type==Type::Gamepad && e.code==0 && e.gamepad.right_trigger==255,"gamepad event");
    r.gamepad(0,Gamepad{},44);
    check(!r.gamepad_state(0).connected && !next(r).gamepad.connected,"gamepad disconnect");
    r.gamepad(4,pad,45);
    check(r.queued()==0,"out-of-range XInput slot accepted");
}
void focus_loss_guarantees_reset() {
    Router r;
    r.key(1,40,false,false,true,1);
    r.mouse_button(1,0,true,2);
    Gamepad pad{};pad.connected=true;r.gamepad(0,pad,3);
    r.set_focus(false,4);
    check(!r.focused() && !r.key_down(40) && !r.button_down(0) &&
          !r.gamepad_state(0).connected,"stale focused input state");
    check(r.queued()==1 && next(r).type==Type::FocusLost,"focus-loss notification");
    r.key(1,40,false,false,true,5);
    r.mouse_move(1,12,12,false,6);
    r.gamepad(0,pad,7);
    check(r.queued()==0,"unfocused input accepted");
    r.set_focus(true,8);
    check(next(r).type==Type::FocusGained && r.focused(),"focus recovery");
    r.key(1,40,false,false,true,9);
    check(next(r).pressed,"key does not resume after focus regain");
}
void queue_overflow_and_reset() {
    Router r;
    for(std::size_t i=0;i<Router::QueueLimit+10u;i++)
        r.mouse_move(1,1,1,false,static_cast<std::uint64_t>(i));
    check(r.queued()==Router::QueueLimit && r.dropped()==10u,"bounded queue overflow");
    r.set_focus(false,900);
    check(r.queued()==1 && next(r).type==Type::FocusLost,
          "focus event lost behind a full queue");
}
}
int main() {
    try {
        keys_extended_and_repeat();e1_and_invalid();
        mouse_modes_buttons_and_wheel();gamepad_changes();
        focus_loss_guarantees_reset();queue_overflow_and_reset();
    }catch(const std::exception& ex) {
        std::cerr<<"native input deterministic tests FAILED: "<<ex.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"native-input-state-machine\","
             "\"scenarios\":6,\"raw_devices_required\":false,"
             "\"focus_cleanup\":true,\"no_emulated_controller\":true,"
             "\"hardware_tested\":false}\n";
    return 0;
}
