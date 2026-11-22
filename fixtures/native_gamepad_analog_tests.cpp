// Original synthetic native Win32/Linux gamepad axes, no HID / XInput emulation.
#include "native_gamepad_analog.hpp"
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace scarlett::analog;
using scarlett::input::FrameSnapshot;
using scarlett::input::Gamepad;
using scarlett::input::Router;
namespace {
void check(bool test,const char* what) {
    if(!test)throw std::runtime_error(what);
}
bool approx(float a,float b,float epsilon=0.0006f) {
    return std::fabs(a-b)<=epsilon;
}
StickConfig no_deadzone(){
    StickConfig c{};
    c.inner_deadzone=0;c.outer_deadzone=0;
    return c;
}
void full_range_and_diagonal(){
    FrameSnapshot frame{};
    Gamepad pad{};pad.connected=true;
    pad.left_x=32767;pad.left_y=-32768;
    pad.right_x=-32768;pad.right_y=32767;
    pad.left_trigger=255;pad.right_trigger=128;
    frame.pads[0]=pad;
    Controller result{};
    auto config=no_deadzone();
    check(sample(frame,0,config,config,result)==Status::Ok,
          "normal full range rejected");
    constexpr float Diagonal=0.70710678f;
    check(result.connected && approx(result.left.x,Diagonal) &&
          approx(result.left.y,-Diagonal) &&
          approx(result.right.x,-Diagonal) &&
          approx(result.right.y,Diagonal),"full diagonal lacked radial clamp");
    check(approx(result.left_trigger,1) &&
          approx(result.right_trigger,128.0f/255.0f),
          "analog triggers were quantized or misnormalized");
    pad.left_y=0;pad.right_y=0;
    frame.pads[0]=pad;
    check(sample(frame,0,config,config,result)==Status::Ok &&
          approx(result.left.x,1.0f) &&
          approx(result.right.x,-1.0f) &&
          approx(result.left.y,0.0f),"positive/negative stick extrema wrong");
}
void exact_deadzone_and_outer_saturation(){
    FrameSnapshot frame{};
    Gamepad pad{};pad.connected=true;
    StickConfig config{};
    config.inner_deadzone=8192;
    config.outer_deadzone=0;
    pad.left_x=4000;frame.pads[0]=pad;
    Controller result{};
    check(sample(frame,0,config,config,result)==Status::Ok &&
          approx(result.left.x,0.0f),"stick drift inside radial deadzone leaked");
    pad.left_x=8192;frame.pads[0]=pad;
    check(sample(frame,0,config,config,result)==Status::Ok &&
          approx(result.left.x,0.0f),"inner deadzone boundary was not neutral");
    pad.left_x=16384;frame.pads[0]=pad;
    check(sample(frame,0,config,config,result)==Status::Ok &&
          approx(result.left.x,8192.0f/(32767.0f-8192.0f)),
          "radial deadzone did not rescale remaining travel");
    auto outer=no_deadzone();
    outer.outer_deadzone=3000;
    pad.left_x=30000;frame.pads[0]=pad;
    check(sample(frame,0,outer,outer,result)==Status::Ok &&
          approx(result.left.x,1.0f),
          "outer deadzone should saturate before mechanical extreme");
}
void curve_inversion_and_independent_sticks(){
    FrameSnapshot frame{};
    Gamepad pad{};pad.connected=true;pad.left_x=12000;
    pad.right_y=-12000;frame.pads[3]=pad;
    auto linear=no_deadzone();
    Controller base{},changed{};
    check(sample(frame,3,linear,linear,base)==Status::Ok,
          "slot3 independent gamepad rejected");
    auto cubic=linear;cubic.curve=Curve::SmoothCubic;
    cubic.invert_x=true;cubic.invert_y=true;
    check(sample(frame,3,cubic,cubic,changed)==Status::Ok &&
          changed.connected && changed.left.x<0 &&
          changed.right.y>0,"curve/invert control failed");
    check(std::fabs(changed.left.x)<std::fabs(base.left.x) &&
          std::fabs(changed.right.y)<std::fabs(base.right.y),
          "cubic response should be gentler near center");
    Controller other{};
    check(sample(frame,0,linear,linear,other)==Status::Ok &&
          !other.connected && approx(other.left.x,0),
          "controller state leaked between slots");
}
void invalid_configuration_is_atomic(){
    FrameSnapshot frame{};
    frame.pads[0].connected=true;
    frame.pads[0].left_x=32767;
    auto valid=no_deadzone();
    Controller destination{};
    check(sample(frame,0,valid,valid,destination)==Status::Ok &&
          destination.left.x==1.0f,"valid setup failed");
    const auto original=destination;
    auto bad=valid;
    bad.inner_deadzone=32767;
    check(sample(frame,0,bad,valid,destination)==Status::InvalidConfig,
          "invalid inner threshold accepted");
    bad=valid;bad.inner_deadzone=20000;bad.outer_deadzone=12767;
    check(sample(frame,0,valid,bad,destination)==Status::InvalidConfig,
          "combined deadzones leaving zero travel accepted");
    bad=valid;bad.curve=static_cast<Curve>(99);
    check(sample(frame,0,bad,valid,destination)==Status::InvalidConfig,
          "unsupported curve accepted");
    check(sample(frame,4,valid,valid,destination)==Status::InvalidSlot,
          "out-of-range slot accepted");
    check(destination.connected==original.connected &&
          destination.left.x==original.left.x &&
          destination.right_trigger==original.right_trigger,
          "rejected profile unexpectedly changed previously valid output");
}
void focus_and_hotplug_neutralization(){
    Router router{};
    Gamepad pad{};pad.connected=true;pad.left_x=32767;pad.left_trigger=200;
    router.gamepad(2,pad,123);
    const auto focused=router.snapshot(124);
    auto config=no_deadzone();
    Controller output{};
    check(sample(focused,2,config,config,output)==Status::Ok &&
          output.connected && approx(output.left.x,1.0f) &&
          output.left_trigger>0.0f,"real snapshot did not propagate analog axes");
    router.set_focus(false,125);
    check(sample(router.snapshot(126),2,config,config,output)==Status::Ok &&
          !output.connected && approx(output.left.x,0) &&
          approx(output.left_trigger,0),
          "focus loss retained held analog/trigger input");
    check(sample(focused,2,config,config,output)==Status::Ok &&
          output.connected && approx(output.left.x,1),
          "prior immutable input snapshot modified by focus loss");
    router.set_focus(true,127);
    check(sample(router.snapshot(128),2,config,config,output)==Status::Ok &&
          !output.connected,"reacquired focus brought stale controller input back");
}
void neutral_and_status_names(){
    FrameSnapshot frame{};
    frame.pads[0].connected=true;
    frame.pads[0].left_y=-1000;
    auto config=no_deadzone();
    Controller output{};
    check(sample(frame,0,config,config,output)==Status::Ok &&
          output.left.y<0,"small negative axis rounded into a zero");
    frame.focused=false;
    check(sample(frame,0,config,config,output)==Status::Ok &&
          !output.connected && output.left.y==0.0f,
          "unfocused snapshot did not suppress analog values");
    check(std::string(status_name(Status::InvalidConfig))=="invalid-config",
          "invalid profile status label");
}
}
int main() {
    try {
        full_range_and_diagonal();
        exact_deadzone_and_outer_saturation();
        curve_inversion_and_independent_sticks();
        invalid_configuration_is_atomic();
        focus_and_hotplug_neutralization();
        neutral_and_status_names();
    }catch(const std::exception& e) {
        std::cerr<<"native analog gamepad test FAILED: "<<e.what()<<'\n';
        return 1;
    }
    std::cout<<"{\"kind\":\"native-PC-gamepad-analog-radial\","
             "\"scenarios\":6,"
             "\"no_extra_smoothing_or_queue\":true,"
             "\"radial_deadzone_and_outer_saturation\":true,"
             "\"cubic_curve_and_inversion\":true,"
             "\"focus_and_disconnect_suppression\":true,"
             "\"platform_hardware_haptics_tested\":false,"
             "\"game_controller_layout_tested\":false}\n";
    return 0;
}
