// Original deterministic keyboard/mouse/gamepad remap and profile recovery tests.
// Synthetic input only, no hardware, injected commands or commercial game formats.
#include "native_action_bindings.hpp"
#include "native_save_slots.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using scarlett::bindings::Binding;
using scarlett::bindings::Map;
using scarlett::bindings::Source;
using scarlett::input::Gamepad;
using scarlett::input::Router;
namespace {
void check(bool yes,const char* why) {
    if(!yes)throw std::runtime_error(why);
}
Binding key(std::uint16_t code,std::uint16_t modifier=0) {
    Binding b{};
    b.source=Source::KeyScan;
    b.code=code;b.modifier_scan=modifier;
    return b;
}
Binding mouse(std::uint16_t button) {
    Binding b{};
    b.source=Source::MouseButton;b.code=button;
    return b;
}
Binding pad_button(std::uint8_t slot,std::uint16_t bit) {
    Binding b{};
    b.source=Source::GamepadButton;b.gamepad_slot=slot;b.code=bit;
    return b;
}
Binding trigger(std::uint8_t slot,bool right,std::uint8_t threshold) {
    Binding b{};
    b.source=right?Source::GamepadRightTrigger:Source::GamepadLeftTrigger;
    b.gamepad_slot=slot;b.threshold=threshold;
    return b;
}
void keyboard_chords_and_mouse_alternatives() {
    Router r;Map map{};
    check(map.bind(2,0,key(0x39,0x11d)),"bind E0 Ctrl+Space");
    check(map.bind(2,1,mouse(0)),"bind alternative mouse button");
    r.key(10,0x39,false,false,true,1); // Space without E0 Ctrl
    check(!map.evaluate(r.snapshot(2)).down(2),"unmodified key shortcut was accepted");
    r.key(11,0x1d,true,false,true,3); // E0 Ctrl on second keyboard
    const auto first=r.snapshot(4);
    check(map.evaluate(first).down(2),"two-device modifier chord not active");
    r.key(11,0x1d,true,false,false,5);
    check(map.evaluate(first).down(2),"old frame snapshot mutated after key release");
    check(!map.evaluate(r.snapshot(6)).down(2),
          "released modifier did not release chord");
    r.mouse_button(12,0,true,7);
    check(map.evaluate(r.snapshot(8)).down(2),"mouse alternative not active");
    r.set_focus(false,9);
    check(!map.evaluate(r.snapshot(10)).down(2),"unfocused action remained active");
    check(map.evaluate(first).down(2),"prior snapshot invalidated by focus loss");
}
void gamepad_buttons_triggers_and_disconnection() {
    Router r;Map map{};
    check(map.bind(3,0,pad_button(1,12)),"bind gamepad button");
    check(map.bind(4,0,trigger(1,false,128)),"bind left trigger");
    check(map.bind(5,0,trigger(1,true,255)),"bind right trigger");
    Gamepad p{};p.connected=true;p.buttons=static_cast<std::uint16_t>(1u<<12);
    p.left_trigger=127;p.right_trigger=254;
    r.gamepad(1,p,1);
    auto state=map.evaluate(r.snapshot(2));
    check(state.down(3) && !state.down(4) && !state.down(5),"initial gamepad thresholds");
    p.left_trigger=128;p.right_trigger=255;
    r.gamepad(1,p,3);
    state=map.evaluate(r.snapshot(4));
    check(state.down(3)&&state.down(4)&&state.down(5),
          "inclusive analog trigger thresholds");
    r.gamepad(1,Gamepad{},5);
    state=map.evaluate(r.snapshot(6));
    check(!state.down(3)&&!state.down(4)&&!state.down(5),
          "disconnected gamepad still mapped to action");
}
void invalid_bindings_do_not_replace_valid() {
    Map map{};
    check(map.bind(7,0,key(0x1e)),"initial valid bind");
    Binding out{};
    check(!map.bind(7,0,key(0)),"invalid keyboard zero scan");
    check(!map.bind(7,0,key(0x1e,0x1e)),"same code and modifier");
    check(!map.bind(7,0,mouse(5)),"invalid mouse button");
    check(!map.bind(7,0,pad_button(4,0)),"invalid pad slot");
    check(!map.bind(7,0,pad_button(0,16)),"invalid gamepad bit");
    check(!map.bind(7,0,trigger(0,true,0)),"zero trigger threshold");
    check(!map.bind(7,0,Binding{static_cast<Source>(255),0,0,0,0}),
          "unsupported source");
    check(!map.bind(32,0,key(0x1e)) && !map.bind(7,4,key(0x1e)),
          "out-of-range action or alternative accepted");
    check(map.get(7,0,out) && out.code==0x1e,"invalid remap overwrote valid binding");
    check(!map.clear(32,0) && !map.clear(7,4),"out-of-range clear succeeded");
    check(map.clear(7,0) && !map.get(7,0,out),"cleared action still bound");
}
std::uint64_t fnv(const std::uint8_t* p,std::size_t n) {
    std::uint64_t h=14695981039346656037ull;
    for(std::size_t i=0;i<n;i++){h^=p[i];h*=1099511628211ull;}
    return h;
}
void update_hash(std::array<std::uint8_t,Map::SerializedBytes>& record) {
    const auto h=fnv(record.data(),record.size()-8u);
    for(std::size_t i=0;i<8;i++)
        record[record.size()-8u+i]=static_cast<std::uint8_t>(h>>(i*8u));
}
void binary_profile_roundtrip_and_strict_rejection() {
    Map original{},recovered{};
    check(original.bind(0,0,key(0x1e)) &&
          original.bind(0,1,mouse(1)) &&
          original.bind(31,3,trigger(3,true,253)),
          "profile setup failed");
    const auto bytes=original.serialize();
    check(bytes.size()==Map::SerializedBytes,"profile size mismatch");
    check(Map::deserialize(bytes.data(),bytes.size(),recovered),
          "profile deserialize rejected valid image");
    check(recovered.serialize()==bytes,"profile canonical serialization mismatch");
    Map destination{};
    check(destination.bind(8,0,key(0x20)),"marker binding");
    auto bad=bytes;
    bad[17]^=0x40u;
    check(!Map::deserialize(bad.data(),bad.size(),destination),
          "bad checksum was accepted");
    check(!Map::deserialize(bytes.data(),bytes.size()-1u,destination),
          "truncated profile was accepted");
    check(!Map::deserialize(nullptr,bytes.size(),destination),
          "null buffer was accepted");
    bad=bytes;bad[8]=2;update_hash(bad);
    check(!Map::deserialize(bad.data(),bad.size(),destination),
          "unknown profile version accepted even with valid checksum");
    bad=bytes;bad[16]=2;update_hash(bad);
    check(!Map::deserialize(bad.data(),bad.size(),destination),
          "invalid enabled flag accepted");
    bad=bytes;bad[17]=255;update_hash(bad);
    check(!Map::deserialize(bad.data(),bad.size(),destination),
          "unsupported source accepted even with valid checksum");
    bad=bytes;bad[16]=0;update_hash(bad);
    check(!Map::deserialize(bad.data(),bad.size(),destination),
          "noncanonical disabled entry accepted");
    Binding marker{};
    check(destination.get(8,0,marker)&&marker.code==0x20,
          "failed profile load replaced existing live profile");
}
struct Temp {
    std::filesystem::path path{};
    Temp(){
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        path=std::filesystem::temp_directory_path()/
             ("scarlett-actions-"+std::to_string(stamp));
        if(!std::filesystem::create_directory(path))
            throw std::runtime_error("temp directory exists");
    }
    ~Temp(){
        std::error_code ignored;
        std::filesystem::remove_all(path,ignored);
    }
};
void persisted_remap_via_existing_native_save_core() {
    Temp temp{};
    Map source{};
    check(source.bind(1,0,key(0x39)) &&
          source.bind(1,1,pad_button(0,12)),"bind persistent action profile");
    const auto encoded=source.serialize();
    const std::vector<std::uint8_t> disk_data(encoded.begin(),encoded.end());
    scarlett::save::Store store(temp.path/"settings with spaces");
    check(store.commit(disk_data,0)==scarlett::save::Status::Ok,
          "transactional profile write failed");
    scarlett::save::Snapshot snapshot{};
    {
        scarlett::save::Store reopened(temp.path/"settings with spaces");
        check(reopened.load(snapshot)==scarlett::save::Status::Ok &&
              snapshot.generation==1 && snapshot.bytes==disk_data,
              "persisted remap snapshot differs on reopen");
    }
    Map loaded{};
    check(Map::deserialize(snapshot.bytes.data(),snapshot.bytes.size(),loaded) &&
          loaded.serialize()==source.serialize(),"profile rehydration changed bindings");
    Router r{};
    r.key(123,0x39,false,false,true,1);
    check(loaded.evaluate(r.snapshot(2)).down(1),
          "recovered keyboard binding was not usable");
}
}
int main() {
    try {
        keyboard_chords_and_mouse_alternatives();
        gamepad_buttons_triggers_and_disconnection();
        invalid_bindings_do_not_replace_valid();
        binary_profile_roundtrip_and_strict_rejection();
        persisted_remap_via_existing_native_save_core();
    }catch(const std::exception& ex){
        std::cerr<<"native action profile test FAILED: "<<ex.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"native-input-remap-profile\","
             "\"scenarios\":5,\"max_actions\":"<<Map::Actions<<","
             "\"alternatives_per_action\":"<<Map::Alternatives<<","
             "\"serialized_bytes\":"<<Map::SerializedBytes<<","
             "\"persistent_snapshot_tested\":true,"
             "\"physical_controllers_or_game_tested\":false}\n";
    return 0;
}
