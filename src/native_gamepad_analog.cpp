#include "native_gamepad_analog.hpp"
#include <algorithm>
#include <cmath>
namespace scarlett::analog {
namespace {
bool valid(const StickConfig& c) noexcept {
    return c.curve==Curve::Linear || c.curve==Curve::SmoothCubic
        ? static_cast<unsigned>(c.inner_deadzone)+
          static_cast<unsigned>(c.outer_deadzone)<32767u
        : false;
}
float axis(std::int16_t v) noexcept {
    return v<0 ? static_cast<float>(v)/32768.0f
               : static_cast<float>(v)/32767.0f;
}
Stick normalize(std::int16_t raw_x,std::int16_t raw_y,
                const StickConfig& config) noexcept {
    const float x=axis(raw_x),y=axis(raw_y);
    const float magnitude=std::sqrt(x*x+y*y);
    const float inner=static_cast<float>(config.inner_deadzone)/32767.0f;
    const float high=1.0f-static_cast<float>(config.outer_deadzone)/32767.0f;
    if(magnitude<=inner)return {};
    float amount=std::clamp((magnitude-inner)/(high-inner),0.0f,1.0f);
    if(config.curve==Curve::SmoothCubic)
        amount=amount*amount*(3.0f-2.0f*amount);
    const float normalized=amount/magnitude;
    return {x*normalized*(config.invert_x?-1.0f:1.0f),
            y*normalized*(config.invert_y?-1.0f:1.0f)};
}
}
Status sample(const input::FrameSnapshot& frame,std::size_t slot,
              const StickConfig& left,const StickConfig& right,
              Controller& output) noexcept {
    if(slot>=frame.pads.size())return Status::InvalidSlot;
    if(!valid(left)||!valid(right))return Status::InvalidConfig;
    Controller result{};
    const auto& pad=frame.pads[slot];
    if(frame.focused && pad.connected) {
        result.connected=true;
        result.left=normalize(pad.left_x,pad.left_y,left);
        result.right=normalize(pad.right_x,pad.right_y,right);
        result.left_trigger=static_cast<float>(pad.left_trigger)/255.0f;
        result.right_trigger=static_cast<float>(pad.right_trigger)/255.0f;
    }
    output=result;
    return Status::Ok;
}
const char* status_name(Status status) noexcept {
    switch(status) {
        case Status::Ok:return "ok";
        case Status::InvalidSlot:return "invalid-slot";
        case Status::InvalidConfig:return "invalid-config";
    }
    return "unknown";
}
} // namespace scarlett::analog
