// Synthetic native gamepad preprocessing tests; no commercial game or controller.
#include "native_analog_input.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using scarlett::analog::Config;
using scarlett::analog::Frame;
using scarlett::analog::sample;
using scarlett::input::Gamepad;
using scarlett::input::Router;
namespace {
void check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
bool near(float a,float b,float tolerance=0.0002f) {
    return std::fabs(a-b)<=tolerance;
}
bool neutral(const Frame& f) {
    return f.left.x==0.0f && f.left.y==0.0f &&
           f.right.x==0.0f && f.right.y==0.0f &&
           f.left_trigger==0.0f && f.right_trigger==0.0f;
}
void device_selection_and_focus() {
    Router r;Config cfg{};
    check(neutral(sample(r.snapshot(0),0,cfg)),"disconnected pad not neutral");
    check(neutral(sample(r.snapshot(0),4,cfg)),"invalid slot not neutral");
    Gamepad pad{};pad.connected=true;pad.left_x=32767;pad.right_trigger=255;
    r.gamepad(2,pad,1);
    check(near(sample(r.snapshot(2),2,cfg).left.x,1.0f),"live pad not sampled");
    check(neutral(sample(r.snapshot(2),1,cfg)),"pad data leaked between slots");
    r.set_focus(false,3);
    check(neutral(sample(r.snapshot(4),2,cfg)),"focus loss retained pad state");
}
void radial_deadzone_full_scale_and_diagonal() {
    Router r;Gamepad pad{};pad.connected=true;
    Config cfg{};cfg.inner_deadzone=0.25f;cfg.outer_deadzone=0.10f;
    pad.left_x=8192;
    r.gamepad(0,pad,0);
    check(near(sample(r.snapshot(1),0,cfg).left.x,0.0f),"inner deadzone leaked drift");
    pad.left_x=16384;r.gamepad(0,pad,2);
    const auto halfway=sample(r.snapshot(3),0,cfg).left;
    check(halfway.x>0.35f && halfway.x<0.40f && halfway.y==0.0f,
          "radial ramp incorrect");
    pad.left_x=32767;r.gamepad(0,pad,4);
    check(near(sample(r.snapshot(5),0,cfg).left.x,1.0f),"positive full scale");
    pad.left_x=-32768;r.gamepad(0,pad,6);
    check(near(sample(r.snapshot(7),0,cfg).left.x,-1.0f),"negative full scale");
    pad.left_x=32767;pad.left_y=32767;r.gamepad(0,pad,8);
    const auto diagonal=sample(r.snapshot(9),0,cfg).left;
    check(near(diagonal.x,diagonal.y) &&
          near(diagonal.x*diagonal.x+diagonal.y*diagonal.y,1.0f),
          "diagonal exceeded unit circle");
}
void cubic_curve_invert_y_and_triggers() {
    Router r;Gamepad pad{};pad.connected=true;pad.left_y=16384;pad.right_x=16384;
    pad.left_trigger=127;pad.right_trigger=255;r.gamepad(0,pad,0);
    Config cfg{};cfg.inner_deadzone=0.0f;cfg.outer_deadzone=0.0f;
    cfg.trigger_floor=127;
    const auto linear=sample(r.snapshot(1),0,cfg);
    check(linear.left.y>0.49f && linear.left.y<0.51f,"linear stick range");
    check(linear.left_trigger==0.0f && linear.right_trigger==1.0f,
          "trigger floor or full scale");
    cfg.cubic_blend=1.0f;cfg.invert_left_y=true;cfg.invert_right_y=true;
    const auto cubic=sample(r.snapshot(2),0,cfg);
    check(cubic.left.y<0.0f && cubic.left.y>-0.14f,"cubic or Y invert failed");
    check(cubic.right.x>0.12f && cubic.right.x<0.14f,"right stick cubic failed");
    cfg.trigger_floor=255;
    check(sample(r.snapshot(3),0,cfg).right_trigger==0.0f,"trigger floor 255 failed");
}
void bad_configs_are_neutral() {
    Router r;Gamepad pad{};pad.connected=true;pad.left_x=32767;
    r.gamepad(0,pad,1);
    Config cfg{};cfg.inner_deadzone=0.9f;cfg.outer_deadzone=0.1f;
    check(!scarlett::analog::valid(cfg) && neutral(sample(r.snapshot(2),0,cfg)),
          "zero effective range accepted");
    cfg={};cfg.cubic_blend=std::numeric_limits<float>::quiet_NaN();
    check(!scarlett::analog::valid(cfg) && neutral(sample(r.snapshot(3),0,cfg)),
          "NaN response accepted");
    cfg={};cfg.inner_deadzone=std::numeric_limits<float>::infinity();
    check(!scarlett::analog::valid(cfg) && neutral(sample(r.snapshot(4),0,cfg)),
          "infinite deadzone accepted");
}
}
int main() {
    try {
        device_selection_and_focus();
        radial_deadzone_full_scale_and_diagonal();
        cubic_curve_invert_y_and_triggers();
        bad_configs_are_neutral();
    } catch(const std::exception& ex) {
        std::cerr<<"native analog preprocessing FAIL: "<<ex.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"native-physical-gamepad-analog\","
               "\"scenarios\":4,\"radial_deadzone\":true,"
               "\"hardware_validated\":false,\"gameplay_integrated\":false}\n";
    return 0;
}
