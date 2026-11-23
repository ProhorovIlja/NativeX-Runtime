// Source-only portable action mapping tests. Not GTA VI game controls.
#include "native_input_bindings.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace scarlett::input;
using namespace scarlett::controls;
namespace {
void check(bool c,const char* why){if(!c)throw std::runtime_error(why);}
const ActionState& state(const Frame& frame,Action action){
    return frame.actions[static_cast<std::size_t>(action)];
}
Frame tick(Mapper& m,Router& r,std::uint64_t n,Context ctx=Context::Gameplay){
    return m.evaluate(r.capture_frame(n),ctx);
}
void keyboard_edges_and_hold(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    r.key(1,0x11,false,false,true,2);
    auto a=tick(m,r,3);
    check(state(a,Action::Forward).pressed && state(a,Action::Forward).down &&
          !state(a,Action::Forward).released,"first W press not observed");
    r.key(1,0x11,false,false,true,4); // Windows auto-repeat suppressed.
    a=tick(m,r,5);
    check(state(a,Action::Forward).down && !state(a,Action::Forward).pressed,
          "held keyboard repeated a press edge");
    r.key(1,0x11,false,false,false,6);
    a=tick(m,r,7);
    check(!state(a,Action::Forward).down && state(a,Action::Forward).released,
          "W release not observed");
    a=tick(m,r,8);
    check(!state(a,Action::Forward).released,"release edge repeated");
}
void multiple_physical_devices_and_disconnection(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    r.key(11,0x39,false,false,true,2);
    r.key(22,0x39,false,false,true,3);
    check(state(tick(m,r,4),Action::Jump).pressed,
          "multiple physical keyboards not aggregated");
    r.key(11,0x39,false,false,false,5);
    const auto still=tick(m,r,6);
    check(state(still,Action::Jump).down && !state(still,Action::Jump).released,
          "one keyboard release cleared other held keyboard");
    r.raw_device_removed(22,7);
    check(state(tick(m,r,8),Action::Jump).released,
          "device disconnect failed to release held action");
}
void mouse_look_wheels_saturation_and_menu_isolation(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    check(m.set_look_settings({1.5f,true}),"valid look settings rejected");
    r.mouse_move(7,14,4,false,2);
    r.mouse_move(7,500,600,true,3); // absolute input never camera motion
    r.mouse_wheel(7,120,false,4);
    r.mouse_wheel(7,-120,true,5);
    r.mouse_button(7,0,true,6);
    const auto frame=tick(m,r,7);
    check(frame.look_x==21.0f && frame.look_y==-6.0f &&
          frame.wheel_vertical==120 && frame.wheel_horizontal==-120 &&
          state(frame,Action::PrimaryAttack).pressed,
          "raw relative look, wheel or mouse button mapping lost");
    const auto next=tick(m,r,8);
    check(next.look_x==0.0f && next.look_y==0.0f &&
          next.wheel_vertical==0 && next.wheel_horizontal==0,
          "camera movement was delivered twice");
    r.mouse_move(7,100,50,false,9);
    auto menu=tick(m,r,10,Context::Menu);
    check(menu.look_x==0.0f && menu.look_y==0.0f &&
          !state(menu,Action::PrimaryAttack).down,
          "menu leaked gameplay raw mouse look");
    r.mouse_move(7,200,100,false,11);
    menu=tick(m,r,12,Context::Menu);
    check(menu.look_x==0.0f && !state(menu,Action::PrimaryAttack).pressed,
          "mouse look was enabled in UI context");
    r.mouse_move(7,std::numeric_limits<std::int32_t>::max(),5,false,13);
    r.mouse_move(7,std::numeric_limits<std::int32_t>::max(),5,false,14);
    const auto huge=tick(m,r,15,Context::Gameplay);
    check(huge.raw_motion_saturated && std::isfinite(huge.look_x),
          "overflowed native raw mouse motion wrapped or went nonfinite");
    check(!m.set_look_settings({std::numeric_limits<float>::infinity(),false}) &&
          !m.set_look_settings({0.0f,false}) &&
          !m.set_look_settings({11.0f,false}),
          "invalid mouse sensitivity accepted");
}
void focus_loss_and_context_switch_prevent_phantom_presses(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    r.key(8,0x01,false,false,true,2); // Escape is Pause in gameplay.
    auto game=tick(m,r,3);
    check(state(game,Action::Pause).pressed,"pause key not recognized");
    auto menu=tick(m,r,4,Context::Menu); // Escape also MenuBack.
    check(state(menu,Action::Pause).released &&
          !state(menu,Action::MenuBack).pressed &&
          !state(menu,Action::MenuBack).down,
          "context swap caused stale or phantom menu action");
    r.key(8,0x01,false,false,false,5);
    tick(m,r,6,Context::Menu);
    r.key(8,0x01,false,false,true,7);
    menu=tick(m,r,8,Context::Menu);
    check(state(menu,Action::MenuBack).pressed,
          "release/repress did not activate menu back");
    r.set_focus(false,9);
    menu=tick(m,r,10,Context::Menu);
    check(!menu.focused && state(menu,Action::MenuBack).released &&
          menu.look_x==0.0f && !state(menu,Action::MenuBack).down,
          "Alt+Tab focus loss left held actions");
    r.set_focus(true,11);
    r.key(8,0x01,false,false,true,12);
    menu=tick(m,r,13,Context::Menu);
    check(!state(menu,Action::MenuBack).pressed,
          "focus regain accepted old held key as a new press");
    r.key(8,0x01,false,false,false,14);
    tick(m,r,15,Context::Menu);
    r.key(8,0x01,false,false,true,16);
    check(state(tick(m,r,17,Context::Menu),Action::MenuBack).pressed,
          "focus recovery failed to rearm after release");
}
void gamepad_physical_buttons_and_disconnect(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    Gamepad pad{};pad.connected=true;pad.buttons=0x1000; // A
    r.gamepad(0,pad,2);
    auto a=tick(m,r,3);
    check(state(a,Action::Jump).pressed &&
          !state(a,Action::MenuAccept).down,
          "XInput gameplay button mapping incorrect");
    r.gamepad(1,pad,4);
    a=tick(m,r,5);
    check(state(a,Action::Jump).down && !state(a,Action::Jump).pressed,
          "second connected pad duplicated press edge");
    r.gamepad(0,Gamepad{},6);
    a=tick(m,r,7);
    check(state(a,Action::Jump).down &&
          !state(a,Action::Jump).released,
          "disconnect of first pad released another pad's control");
    r.gamepad(1,Gamepad{},8);
    check(state(tick(m,r,9),Action::Jump).released,
          "all pads disconnected but jump remained held");
    // Any-pad mapping is an original control choice, not per-user multiplayer.
}
void conflicts_validation_and_rebind(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    check(m.bind(Action::Jump,0,{Source::ScanCode,0x11,Context::Gameplay,true})==
          BindResult::Conflict,"same-context W collision accepted");
    check(m.bind(Action::MenuAccept,0,{Source::ScanCode,0x11,Context::Menu,true})==
          BindResult::Ok,"cross-context W share rejected");
    check(m.bind(Action::Forward,2,{Source::ScanCode,0x44,Context::Gameplay,true})==
          BindResult::InvalidSlot,"third binding slot accepted");
    check(m.bind(static_cast<Action>(200),0,{Source::ScanCode,0x44,Context::Gameplay,true})==
          BindResult::InvalidAction,"out-of-range action accepted");
    check(m.bind(Action::Jump,0,{Source::ScanCode,0,Context::Gameplay,true})==
          BindResult::InvalidInput,"zero key accepted");
    check(m.bind(Action::Jump,0,{Source::MouseButton,5,Context::Gameplay,true})==
          BindResult::InvalidInput,"sixth mouse button accepted");
    check(m.bind(Action::Jump,0,{Source::GamepadButton,0x3000,Context::Gameplay,true})==
          BindResult::InvalidInput,"multiple XInput mask bits accepted");
    check(m.bind(Action::Jump,0,{static_cast<Source>(222),1,Context::Gameplay,true})==
          BindResult::InvalidInput,"unknown source accepted");
    check(m.bind(Action::Jump,0,{Source::ScanCode,0x39,static_cast<Context>(8),true})==
          BindResult::InvalidInput,"unknown context accepted");
    // A successful configuration edit deliberately rearms on the next frame.
    tick(m,r,1);
    r.key(2,0x39,false,false,true,2);
    check(state(tick(m,r,3),Action::Jump).pressed,"prior key still mapped");
    check(m.bind(Action::Jump,0,{Source::ScanCode,0x2e,Context::Gameplay,true})==
          BindResult::Ok,"rebind C key failed");
    r.key(2,0x2e,false,false,true,4);
    const auto first=tick(m,r,5);
    check(state(first,Action::Jump).released &&
          !state(first,Action::Jump).pressed,
          "rebind of held key caused spurious press");
    r.key(2,0x2e,false,false,false,6);
    tick(m,r,7);
    r.key(2,0x2e,false,false,true,8);
    check(state(tick(m,r,9),Action::Jump).pressed,
          "remapped key did not work after release");
    check(m.clear(Action::Jump,0)==BindResult::Ok,"clear binding failed");
    const auto cleared=tick(m,r,10);
    check(!state(cleared,Action::Jump).down &&
          state(cleared,Action::Jump).released,
          "clear held key left state pressed");
    check(!m.binding(Action::Jump,0).enabled &&
          m.binding(static_cast<Action>(255),0).enabled==false,
          "cleared or invalid mapping returned active data");
}
void raw_queue_overflow_does_not_drop_camera(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    for(std::uint64_t i=0;i<Router::QueueLimit+128;i++)
        r.mouse_move(1,1,-1,false,i+2);
    const auto result=tick(m,r,1000);
    check(result.look_x==static_cast<float>(Router::QueueLimit+128) &&
          result.look_y==-static_cast<float>(Router::QueueLimit+128) &&
          r.dropped()>=128,
          "diagnostic ring overflow dropped frame mouse motion");
    check(tick(m,r,1001).look_x==0.0f,
          "overflow frame motion delivered twice");
}
void short_keyboard_tap_before_frame_is_observable(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    r.key(1,0x11,false,false,true,2);
    r.key(1,0x11,false,false,false,3);
    const auto peek=r.snapshot(4);
    check(peek.key_press_edges.test(0x11) &&
          peek.key_release_edges.test(0x11) && !peek.key_down(0x11),
          "short keyboard press+release was lost before frame snapshot");
    const auto frame=tick(m,r,5);
    check(state(frame,Action::Forward).pressed &&
          state(frame,Action::Forward).released &&
          !state(frame,Action::Forward).down,
          "complete short keyboard tap did not trigger both edges");
    const auto again=tick(m,r,6);
    check(!state(again,Action::Forward).pressed &&
          !state(again,Action::Forward).released,
          "short keyboard tap delivered more than once");
    // Multiple events for the same key collapse to at-least-one edge of
    // each kind, not an unbounded count of taps.
    r.key(1,0x11,false,false,true,7);
    r.key(1,0x11,false,false,false,8);
    r.key(1,0x11,false,false,true,9);
    const auto held=tick(m,r,10);
    check(state(held,Action::Forward).down &&
          state(held,Action::Forward).pressed &&
          !state(held,Action::Forward).released,
          "pressed final state incorrectly emitted a full tap release");
}
void short_mouse_and_gamepad_taps_before_frame(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    r.mouse_button(9,0,true,2);
    r.mouse_button(9,0,false,3);
    Gamepad pad{};pad.connected=true;pad.buttons=0x1000;
    r.gamepad(0,pad,4);
    pad.buttons=0;
    r.gamepad(0,pad,5);
    const auto snapshot=r.snapshot(6);
    check(snapshot.mouse_press_edges.test(0) &&
          snapshot.mouse_release_edges.test(0) &&
          (snapshot.gamepad_press_edges&0x1000)!=0 &&
          (snapshot.gamepad_release_edges&0x1000)!=0,
          "mouse or pad rapid tap edges missing");
    const auto frame=tick(m,r,7);
    check(state(frame,Action::PrimaryAttack).pressed &&
          state(frame,Action::PrimaryAttack).released &&
          !state(frame,Action::PrimaryAttack).down &&
          state(frame,Action::Jump).pressed &&
          state(frame,Action::Jump).released &&
          !state(frame,Action::Jump).down,
          "mouse or gamepad short tap was dropped");
    const auto next=tick(m,r,8);
    check(!state(next,Action::PrimaryAttack).pressed &&
          !state(next,Action::Jump).pressed,
          "mouse or gamepad pulse replayed after capture");
}
void aggregate_device_edges_and_overflow(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    r.key(10,0x39,false,false,true,2);
    r.key(20,0x39,false,false,true,3);
    check(state(tick(m,r,4),Action::Jump).pressed,
          "aggregate keyboard press did not reach mapper");
    r.key(10,0x39,false,false,false,5);
    auto peek=r.snapshot(6);
    check(!peek.key_release_edges.test(0x39),
          "releasing one of two active keyboards emitted an aggregate release");
    check(state(tick(m,r,7),Action::Jump).down,
          "action released although another keyboard still holds jump");
    r.raw_device_removed(20,8);
    check(r.snapshot(9).key_release_edges.test(0x39) &&
          state(tick(m,r,10),Action::Jump).released,
          "removing last keyboard did not report release");
    // Make diagnostic ring full before a button tap. The independent
    // per-frame edges must still survive dropped diagnostic records.
    for(std::uint64_t i=0;i<Router::QueueLimit+32;i++)
        r.mouse_move(30,1,0,false,11+i);
    r.mouse_button(30,0,true,900);
    r.mouse_button(30,0,false,901);
    check(r.dropped()>=32,"synthetic diagnostic ring did not overflow");
    const auto full=tick(m,r,902);
    check(state(full,Action::PrimaryAttack).pressed &&
          state(full,Action::PrimaryAttack).released &&
          !state(full,Action::PrimaryAttack).down,
          "quick click lost when bounded diagnostic ring overflowed");
}
void focus_rearm_and_rebind_drop_old_short_taps(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1);
    r.key(1,0x11,false,false,true,2);
    r.key(1,0x11,false,false,false,3);
    r.set_focus(false,4);
    const auto unfocused=tick(m,r,5);
    check(!unfocused.focused && !state(unfocused,Action::Forward).pressed,
          "old subframe input survived loss of focus");
    r.set_focus(true,6);
    r.key(1,0x11,false,false,true,7);
    r.key(1,0x11,false,false,false,8);
    check(!state(tick(m,r,9),Action::Forward).pressed,
          "first focus reacquisition frame triggered stale tap");
    r.key(1,0x11,false,false,true,10);
    r.key(1,0x11,false,false,false,11);
    check(state(tick(m,r,12),Action::Forward).pressed,
          "fresh post-focus rapid tap lost");
    r.key(1,0x39,false,false,true,13);
    r.key(1,0x39,false,false,false,14);
    check(m.bind(Action::Jump,0,{Source::ScanCode,0x2e,
          Context::Gameplay,true})==BindResult::Ok,
          "jump remapping failed");
    check(!state(tick(m,r,15),Action::Jump).pressed,
          "rebind replayed an unrelated old key press");
    r.key(1,0x2e,false,false,true,16);
    r.key(1,0x2e,false,false,false,17);
    check(state(tick(m,r,18),Action::Jump).pressed,
          "new binding failed to capture fresh short tap");
}
void menu_binding_and_name_examples(){
    Mapper m=Mapper::desktop_example();Router r{};
    tick(m,r,1,Context::Menu);
    r.key(11,0x1c,false,false,true,2);
    const auto frame=tick(m,r,3,Context::Menu);
    check(state(frame,Action::MenuAccept).pressed &&
          !state(frame,Action::Pause).down,
          "menu enter not isolated from gameplay");
    check(name(Action::Forward)[0]=='f' &&
          name(BindResult::Conflict)[0]=='c',
          "public action names mismatched");
}
}
int main() {
    try{
        keyboard_edges_and_hold();
        multiple_physical_devices_and_disconnection();
        mouse_look_wheels_saturation_and_menu_isolation();
        focus_loss_and_context_switch_prevent_phantom_presses();
        gamepad_physical_buttons_and_disconnect();
        conflicts_validation_and_rebind();
        raw_queue_overflow_does_not_drop_camera();
        short_keyboard_tap_before_frame_is_observable();
        short_mouse_and_gamepad_taps_before_frame();
        aggregate_device_edges_and_overflow();
        focus_rearm_and_rebind_drop_old_short_taps();
        menu_binding_and_name_examples();
    }catch(const std::exception& e){
        std::cerr<<"native PC action binding FAIL: "<<e.what()<<'\n';
        return 1;
    }
    std::cout<<"{\"kind\":\"native-pc-remappable-actions\","
             "\"scenarios\":12,\"synthetic_input_only\":true,"
             "\"mouse_native_not_analog_emulated\":true,"
             "\"no_game_binary\":true,\"no_global_hooks\":true}\n";
    return 0;
}
