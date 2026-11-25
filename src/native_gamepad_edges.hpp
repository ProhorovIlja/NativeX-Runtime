#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace scarlett::input {
// Fixed-capacity per-slot transitions. Single input-owner thread only.
// Multiple changes within a capture interval may set both edges. Slot
// identity is ephemeral; the backend must separately track physical devices.
struct PadEdges {
    static constexpr std::size_t Slots=4;
    std::array<std::uint16_t,Slots> press{},release{};
    bool record(std::size_t slot,std::uint16_t before,std::uint16_t after) noexcept {
        if(slot>=Slots)return false;
        const auto changed=static_cast<std::uint16_t>(before^after);
        press[slot]|=static_cast<std::uint16_t>(changed&after);
        release[slot]|=static_cast<std::uint16_t>(changed&before);
        return true;
    }
    void clear() noexcept {press.fill(0);release.fill(0);}
};
}
