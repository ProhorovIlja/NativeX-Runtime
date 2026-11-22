#include "native_input_core.hpp"
#include <cstdint>
#include <limits>
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
void multiple_physical_keyboards_and_mice() {
    Router r;
    r.key(100,0x1e,false,false,true,10);
    r.key(200,0x1e,false,false,true,11);
    check(r.queued()==2 && r.key_down(0x1e) &&
          r.device_key_down(100,0x1e) && r.device_key_down(200,0x1e),
          "second physical keyboard press incorrectly deduplicated");
    r.key(100,0x1e,false,false,false,12);
    check(r.key_down(0x1e) && !r.device_key_down(100,0x1e) &&
          r.device_key_down(200,0x1e),"first keyboard release cleared second keyboard");
    r.mouse_button(300,0,true,13);
    r.mouse_button(400,0,true,14);
    r.mouse_button(300,0,false,15);
    check(r.button_down(0) && !r.device_button_down(300,0) &&
          r.device_button_down(400,0),"first mouse release cleared second mouse");
    r.raw_device_removed(200,16);
    check(!r.key_down(0x1e) && !r.device_key_down(200,0x1e),
          "unplugged keyboard left held key");
    r.raw_device_removed(400,17);
    check(!r.button_down(0),"unplugged mouse left held button");
    bool saw_disconnect_200=false,saw_disconnect_400=false;
    Event event{};
    while(r.pop(event)) {
        if(event.type==Type::DeviceDisconnected && event.device==200)
            saw_disconnect_200=true;
        if(event.type==Type::DeviceDisconnected && event.device==400)
            saw_disconnect_400=true;
    }
    check(saw_disconnect_200 && saw_disconnect_400,"missing device removal events");
}
void physical_device_capacity_and_reuse() {
    Router r;
    for(std::size_t i=0;i<Router::RawDeviceSlots;i++)
        r.key(static_cast<std::uint64_t>(i+1),0x20,false,false,true,
              static_cast<std::uint64_t>(i+1));
    check(r.queued()==Router::RawDeviceSlots && r.key_down(0x20),
          "max Raw Input device capacity");
    r.key(900,0x21,false,false,true,101);
    check(r.dropped()==1 && !r.key_down(0x21),
          "exhausted raw device table must reject new device");
    r.raw_device_removed(1,102);
    check(r.key_down(0x20),"device removal cleared a different device key");
    r.key(900,0x21,false,false,true,103);
    check(r.key_down(0x21) && r.device_key_down(900,0x21),
          "physical device slot not reusable after unplug");
}
void saturated_queue_disconnect_is_observable() {
    Router r;
    r.key(700,40,false,false,true,1);
    for(std::size_t i=0;i<Router::QueueLimit;i++)
        r.mouse_move(700,1,1,false,static_cast<std::uint64_t>(i+2));
    check(r.queued()==Router::QueueLimit,"queue did not fill");
    r.raw_device_removed(700,900);
    check(!r.key_down(40) && r.queued()==1 &&
          r.dropped()>=Router::QueueLimit &&
          next(r).type==Type::DeviceDisconnected,
          "unplug with full queue did not force observable state reset");
}
void immutable_frame_state_snapshots() {
    Router r;
    Gamepad pad{};pad.connected=true;pad.left_x=2468;pad.buttons=0x1000;
    r.key(10,0x1d,false,false,true,1);
    r.key(20,0x1d,false,false,true,2);
    r.mouse_button(30,0,true,3);
    r.gamepad(0,pad,4);
    const std::size_t queued_before=r.queued();
    const auto first=r.snapshot(50);
    check(first.captured_ticks==50 && first.focused && first.key_down(0x1d) &&
          first.button_down(0) && first.pads[0].left_x==2468 &&
          first.pads[0].connected && first.pads[0].buttons==0x1000,
          "immutable frame snapshot omitted aggregated input");
    check(first.dropped_events==0 && r.queued()==queued_before,
          "snapshot must never consume queued Raw Input events");
    r.key(10,0x1d,false,false,false,5);
    check(r.snapshot(51).key_down(0x1d),
          "first keyboard release incorrectly clears other keyboard");
    r.raw_device_removed(20,6);
    r.mouse_button(30,0,false,7);
    r.gamepad(0,Gamepad{},8);
    const auto second=r.snapshot(52);
    check(!second.key_down(0x1d) && !second.button_down(0) &&
          !second.pads[0].connected && second.captured_ticks==52,
          "post-disconnect frame snapshot retained released state");
    check(first.key_down(0x1d) && first.button_down(0) &&
          first.pads[0].connected,"earlier captured snapshot mutated");
}
void snapshot_focus_and_queue_overflow() {
    Router r;
    r.key(100,0x21,false,false,true,1);
    for(std::size_t i=0;i<Router::QueueLimit+5u;i++)
        r.mouse_move(100,1,1,false,static_cast<std::uint64_t>(2+i));
    const auto before=r.snapshot(2000);
    check(before.key_down(0x21) && before.dropped_events==6 &&
          before.focused,"snapshot did not preserve overflow accounting");
    r.set_focus(false,2001);
    const auto after=r.snapshot(2002);
    check(!after.focused && !after.key_down(0x21) &&
          !after.button_down(0),"snapshot did not reflect focus-reset state");
    check(before.key_down(0x21) && before.focused,
          "first snapshot was not independent of Router backing storage");
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

void mouse_frame_relative_absolute_wheel_distinction() {
    Router r;
    r.mouse_move(10,12,-9,false,1);
    r.mouse_move(11,-7,20,false,2);
    r.mouse_move(11,6000,4000,true,3); // Absolute HID coordinates, not deltas.
    r.mouse_wheel(10,120,false,4);
    r.mouse_wheel(11,-40,false,5);
    r.mouse_wheel(11,15,true,6);
    const auto frame=r.snapshot(100);
    check(frame.motion.relative_x==5 && frame.motion.relative_y==11 &&
          frame.motion.wheel_vertical==80 &&
          frame.motion.wheel_horizontal==15 &&
          frame.motion.packets==5 && !frame.motion.saturated,
          "raw mouse deltas were mixed with absolute coordinates or wheel axes");
    check(frame.captured_ticks==100 && r.queued()==6,
          "motion preview should not remove Raw Input events");
    check(next(r).type==Type::MouseRelative &&
          next(r).type==Type::MouseRelative &&
          next(r).type==Type::MouseAbsolute,
          "absolute/relative event provenance was not preserved");
}
void mouse_frame_capture_is_exactly_once() {
    Router r;
    r.key(1,0x1e,false,false,true,1);
    r.mouse_button(2,0,true,2);
    r.mouse_move(2,10,-3,false,3);
    r.mouse_wheel(2,120,false,4);
    const auto preview=r.snapshot(5);
    const auto queued=r.queued();
    const auto first=r.capture_frame(6);
    check(first.key_down(0x1e) && first.button_down(0) &&
          first.motion.relative_x==10 && first.motion.relative_y==-3 &&
          first.motion.wheel_vertical==120 &&
          first.motion.packets==2 && first.captured_ticks==6,
          "frame capture lost held state or movement");
    check(r.queued()==queued && r.key_down(0x1e),
          "capture incorrectly drains diagnostics or held buttons");
    const auto second=r.capture_frame(7);
    check(second.motion.relative_x==0 && second.motion.relative_y==0 &&
          second.motion.wheel_vertical==0 && second.motion.packets==0 &&
          !second.motion.saturated,"captured motion replayed on next frame");
    check(preview.motion.relative_x==10 && preview.motion.packets==2 &&
          first.motion.relative_x==10,"prior snapshots mutated after capture");
    r.mouse_move(3,-4,6,false,8);
    check(r.capture_frame(9).motion.relative_x==-4 &&
          r.capture_frame(10).motion.packets==0,
          "new motion after capture did not form next frame");
}
void mouse_frame_not_lost_when_event_ring_overflows() {
    Router r;
    for(std::size_t i=0;i<Router::QueueLimit+127u;i++)
        r.mouse_move(1,1,-2,false,static_cast<std::uint64_t>(i+1));
    check(r.queued()==Router::QueueLimit && r.dropped()==127,
          "raw diagnostic event ring should stay bounded");
    const auto captured=r.capture_frame(1000);
    check(captured.motion.relative_x==
              static_cast<std::int32_t>(Router::QueueLimit+127u) &&
          captured.motion.relative_y==
              -2*static_cast<std::int32_t>(Router::QueueLimit+127u) &&
          captured.motion.packets==Router::QueueLimit+127u &&
          captured.dropped_events==127,
          "event ring overflow incorrectly discarded frame mouse displacement");
    r.set_focus(false,1001);
    check(r.queued()==1 && r.snapshot(1002).motion.packets==0,
          "focus loss must discard old motion and old diagnostic events");
    r.mouse_move(1,99,99,false,1003);
    check(r.capture_frame(1004).motion.packets==0,"background motion was accepted");
    r.set_focus(true,1005);
    r.mouse_move(1,7,-8,false,1006);
    check(r.capture_frame(1007).motion.relative_x==7 &&
          r.capture_frame(1008).motion.packets==0,"reacquired focus reused stale motion");
    check(captured.motion.packets==Router::QueueLimit+127u,
          "old snapshot mutated after focus loss");
}
void mouse_frame_signed_saturation_never_wraps() {
    Router r;
    const auto max=std::numeric_limits<std::int32_t>::max();
    const auto min=std::numeric_limits<std::int32_t>::min();
    r.mouse_move(1,max,min,false,1);
    r.mouse_move(2,max,min,false,2);
    r.mouse_wheel(1,max,false,3);
    r.mouse_wheel(1,max,false,4);
    const auto captured=r.capture_frame(5);
    check(captured.motion.relative_x==max && captured.motion.relative_y==min &&
          captured.motion.wheel_vertical==max &&
          captured.motion.packets==4 && captured.motion.saturated,
          "overflowed mouse displacement or wheel wrapped signed 32-bit");
    r.mouse_move(2,min,max,false,6);
    r.mouse_wheel(2,min,true,7);
    r.mouse_wheel(2,min,true,8);
    const auto negative=r.capture_frame(9);
    check(negative.motion.relative_x==min && negative.motion.relative_y==max &&
          negative.motion.wheel_horizontal==min && negative.motion.saturated,
          "negative motion/wheel saturation wrapped or underflowed");
    check(r.capture_frame(10).motion.packets==0 &&
          !r.capture_frame(11).motion.saturated,
          "motion overflow flag survived a frame boundary");
}
}
int main() {
    try {
        keys_extended_and_repeat();e1_and_invalid();
        mouse_modes_buttons_and_wheel();gamepad_changes();
        focus_loss_guarantees_reset();queue_overflow_and_reset();
        multiple_physical_keyboards_and_mice();
        physical_device_capacity_and_reuse();
        saturated_queue_disconnect_is_observable();
        immutable_frame_state_snapshots();
        snapshot_focus_and_queue_overflow();
        mouse_frame_relative_absolute_wheel_distinction();
        mouse_frame_capture_is_exactly_once();
        mouse_frame_not_lost_when_event_ring_overflows();
        mouse_frame_signed_saturation_never_wraps();
    }catch(const std::exception& ex) {
        std::cerr<<"native input deterministic tests FAILED: "<<ex.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"native-input-state-machine\","
             "\"scenarios\":15,\"raw_devices_required\":false,"
             "\"focus_cleanup\":true,\"no_emulated_controller\":true,"
             "\"hardware_tested\":false}\n";
    return 0;
}
