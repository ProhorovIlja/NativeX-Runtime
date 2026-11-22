#include "native_action_bindings.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace scarlett::bindings {
namespace {
constexpr std::array<std::uint8_t,8> Magic{{'S','C','B','I','N','D','0','1'}};
constexpr std::uint64_t Offset=14695981039346656037ull;
constexpr std::uint64_t Prime=1099511628211ull;
constexpr std::size_t Header=16u;
constexpr std::size_t Entry=8u;
std::uint64_t hash(const std::uint8_t* data,std::size_t n) noexcept {
    std::uint64_t result=Offset;
    for(std::size_t i=0;i<n;i++){result^=data[i];result*=Prime;}
    return result;
}
void put16(std::uint8_t* p,std::uint16_t value) noexcept {
    p[0]=static_cast<std::uint8_t>(value&255u);
    p[1]=static_cast<std::uint8_t>(value>>8u);
}
std::uint16_t get16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0]|(static_cast<unsigned>(p[1])<<8u));
}
void put64(std::uint8_t* p,std::uint64_t n) noexcept {
    for(std::size_t i=0;i<8;i++)p[i]=static_cast<std::uint8_t>(n>>(i*8));
}
std::uint64_t get64(const std::uint8_t* p) noexcept {
    std::uint64_t n=0;
    for(std::size_t i=0;i<8;i++)n|=static_cast<std::uint64_t>(p[i])<<(i*8);
    return n;
}
}
bool Map::valid(Binding b) noexcept {
    if(b.modifier_scan>=input::Router::KeySlots)return false;
    switch(b.source) {
        case Source::KeyScan:
            return b.code>0 && b.code<input::Router::KeySlots &&
                   b.code!=b.modifier_scan && b.gamepad_slot==0 &&
                   b.threshold==0;
        case Source::MouseButton:
            return b.code<5 && b.gamepad_slot==0 && b.threshold==0;
        case Source::GamepadButton:
            return b.code<16 && b.gamepad_slot<input::Router::GamepadSlots &&
                   b.threshold==0;
        case Source::GamepadLeftTrigger:
        case Source::GamepadRightTrigger:
            return b.code==0 && b.gamepad_slot<input::Router::GamepadSlots &&
                   b.threshold>0;
    }
    return false;
}
bool Map::bind(std::uint8_t action,std::uint8_t alternative,Binding b) noexcept {
    if(action>=Actions || alternative>=Alternatives || !valid(b))return false;
    slots_[static_cast<std::size_t>(action)*Alternatives+alternative]={true,b};
    return true;
}
bool Map::clear(std::uint8_t action,std::uint8_t alternative) noexcept {
    if(action>=Actions || alternative>=Alternatives)return false;
    slots_[static_cast<std::size_t>(action)*Alternatives+alternative]=Slot{};
    return true;
}
bool Map::get(std::uint8_t action,std::uint8_t alternative,Binding& out) const noexcept {
    if(action>=Actions || alternative>=Alternatives)return false;
    const auto& slot=slots_[static_cast<std::size_t>(action)*Alternatives+alternative];
    if(!slot.enabled)return false;
    out=slot.binding;
    return true;
}
bool Map::activated(Binding b,const input::FrameSnapshot& frame) noexcept {
    if(b.modifier_scan && !frame.key_down(b.modifier_scan))return false;
    switch(b.source) {
        case Source::KeyScan:return frame.key_down(b.code);
        case Source::MouseButton:return frame.button_down(static_cast<std::uint8_t>(b.code));
        case Source::GamepadButton:{
            const auto& pad=frame.pads[b.gamepad_slot];
            return pad.connected && (pad.buttons&(1u<<b.code))!=0;
        }
        case Source::GamepadLeftTrigger:{
            const auto& pad=frame.pads[b.gamepad_slot];
            return pad.connected && pad.left_trigger>=b.threshold;
        }
        case Source::GamepadRightTrigger:{
            const auto& pad=frame.pads[b.gamepad_slot];
            return pad.connected && pad.right_trigger>=b.threshold;
        }
    }
    return false;
}
ActionState Map::evaluate(const input::FrameSnapshot& frame) const noexcept {
    ActionState output{};
    if(!frame.focused)return output;
    for(std::size_t action=0;action<Actions;action++){
        for(std::size_t alt=0;alt<Alternatives;alt++){
            const auto& slot=slots_[action*Alternatives+alt];
            if(slot.enabled && activated(slot.binding,frame)){
                output.pressed.set(action);
                break;
            }
        }
    }
    return output;
}
std::array<std::uint8_t,Map::SerializedBytes> Map::serialize() const noexcept {
    std::array<std::uint8_t,SerializedBytes> data{};
    for(std::size_t i=0;i<Magic.size();i++)data[i]=Magic[i];
    data[8]=1u;  // v1 little-endian u32; bytes 9..11 are zero
    // Header bytes 12..15 are reserved and must remain zero.
    for(std::size_t i=0;i<slots_.size();i++){
        if(!slots_[i].enabled)continue;
        const auto& b=slots_[i].binding;
        std::uint8_t* e=data.data()+Header+i*Entry;
        e[0]=1u;
        e[1]=static_cast<std::uint8_t>(b.source);
        put16(e+2,b.code);
        put16(e+4,b.modifier_scan);
        e[6]=b.gamepad_slot;
        e[7]=b.threshold;
    }
    put64(data.data()+SerializedBytes-8u,hash(data.data(),SerializedBytes-8u));
    return data;
}
bool Map::deserialize(const std::uint8_t* bytes,std::size_t length,
                      Map& destination) noexcept {
    if(!bytes || length!=SerializedBytes)return false;
    for(std::size_t i=0;i<Magic.size();i++)if(bytes[i]!=Magic[i])return false;
    if(bytes[8]!=1u)return false;
    for(std::size_t i=9;i<Header;i++)if(bytes[i]!=0)return false;
    if(get64(bytes+SerializedBytes-8u)!=hash(bytes,SerializedBytes-8u))
        return false;
    Map candidate{};
    for(std::size_t i=0;i<Actions*Alternatives;i++){
        const auto* e=bytes+Header+i*Entry;
        if(e[0]==0) {
            for(std::size_t k=1;k<Entry;k++)if(e[k]!=0)return false;
            continue;
        }
        if(e[0]!=1)return false;
        Binding b{};
        b.source=static_cast<Source>(e[1]);
        b.code=get16(e+2);
        b.modifier_scan=get16(e+4);
        b.gamepad_slot=e[6];
        b.threshold=e[7];
        if(!candidate.bind(static_cast<std::uint8_t>(i/Alternatives),
                           static_cast<std::uint8_t>(i%Alternatives),b))
            return false;
    }
    destination=candidate;
    return true;
}
} // namespace scarlett::bindings
