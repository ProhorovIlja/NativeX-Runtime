#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace scarlett::wave {
// CPU model of two 32-lane partitions of one logical compute wave.
// Integer-only; inactive reads are undefined, represented by nullopt.
using Mask=std::uint64_t;
using Values=std::array<std::uint32_t,64>;
struct SplitMask {
    std::uint32_t lo=0,hi=0;
    static constexpr SplitMask from(Mask m) noexcept {
        return {static_cast<std::uint32_t>(m),static_cast<std::uint32_t>(m>>32)};
    }
    constexpr Mask joined() const noexcept {return Mask{lo}|(Mask{hi}<<32);}
    constexpr bool active(unsigned lane) const noexcept {
        return lane<64 && (((lane<32?lo:hi)>>(lane%32))&1u)!=0;
    }
};
constexpr unsigned count32(std::uint32_t x) noexcept {
    unsigned n=0;while(x){x&=x-1u;++n;}return n;
}
constexpr std::uint32_t below32(unsigned lane) noexcept {
    return lane>=32?UINT32_MAX:((std::uint32_t{1}<<lane)-1u);
}
constexpr Mask ballot(SplitMask exec,SplitMask predicate) noexcept {
    return SplitMask{exec.lo&predicate.lo,exec.hi&predicate.hi}.joined();
}
inline std::optional<unsigned> prefix_count(SplitMask exec,SplitMask predicate,unsigned lane) noexcept {
    if(!exec.active(lane))return std::nullopt;
    const auto low=exec.lo&predicate.lo,high=exec.hi&predicate.hi;
    return lane<32?count32(low&below32(lane)):count32(low)+count32(high&below32(lane-32));
}
inline std::optional<std::uint32_t> read_lane(const Values& values,SplitMask exec,unsigned dst,unsigned src) noexcept {
    if(!exec.active(dst)||!exec.active(src))return std::nullopt;
    return values[src];
}
inline std::optional<unsigned> first_lane(SplitMask exec) noexcept {
    for(unsigned i=0;i<64;++i)if(exec.active(i))return i;
    return std::nullopt;
}
// Unsigned 32-bit addition wraps modulo 2^32. No floating-point ordering claims.
inline std::uint32_t half_sum(const Values& values,std::uint32_t mask,unsigned half,unsigned end=32) noexcept {
    if(half>1||end>32)return 0;
    std::uint32_t result=0;
    for(unsigned i=0;i<end;++i)if((mask>>i)&1u)result+=values[half*32+i];
    return result;
}
inline std::optional<std::uint32_t> active_sum(const Values& values,SplitMask exec,unsigned lane) noexcept {
    if(!exec.active(lane))return std::nullopt;
    return half_sum(values,exec.lo,0)+half_sum(values,exec.hi,1);
}
inline std::optional<std::uint32_t> prefix_sum(const Values& values,SplitMask exec,unsigned lane) noexcept {
    if(!exec.active(lane))return std::nullopt;
    if(lane<32)return half_sum(values,exec.lo,0,lane);
    return half_sum(values,exec.lo,0)+half_sum(values,exec.hi,1,lane-32);
}
}
