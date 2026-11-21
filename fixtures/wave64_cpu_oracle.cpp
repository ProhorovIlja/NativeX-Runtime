// Original, source-built 64-lane CPU oracle and 2x32-lane translation model.
// These are CPU correctness fixtures, NOT GPU hardware execution or a title test.
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using Values = std::array<uint32_t, 64>;
struct Wave {
    Values value{};
    uint64_t active = 0;
};
struct Read {
    bool defined;
    uint32_t value;
};
constexpr uint64_t bit(unsigned lane) { return uint64_t(1) << lane; }
bool active(const Wave& w, unsigned lane) { return (w.active & bit(lane)) != 0; }
uint32_t wrap_add(uint32_t a, uint32_t b) { return uint32_t(a + b); }

// Reference: a single logical wave64; reading an inactive source is undefined.
Read oracle_read(const Wave& w, unsigned destination, unsigned source) {
    if (!active(w, destination) || !active(w, source)) return {false, 0};
    return {true, w.value[source]};
}
uint32_t oracle_total(const Wave& w) {
    uint32_t sum = 0;
    for (unsigned i=0;i<64;i++) if (active(w,i)) sum = wrap_add(sum, w.value[i]);
    return sum;
}
uint32_t oracle_prefix(const Wave& w, unsigned destination) {
    uint32_t sum = 0;
    for (unsigned i=0;i<=destination;i++)
        if (active(w,i)) sum = wrap_add(sum,w.value[i]);
    return sum;
}

// Translation model: two physical wave32 groups exchange values across a
// shared bridge and combine their partial results. Not executable GPU code.
struct Split {
    std::array<uint32_t,32> lower{},upper{};
    uint32_t lower_total=0,upper_total=0;
    std::array<uint32_t,32> lower_prefix{},upper_prefix{};
    uint64_t mask=0;
};
Split split(const Wave& w) {
    Split s{};
    s.mask=w.active;
    uint32_t lo=0,hi=0;
    for (unsigned i=0;i<32;i++) {
        s.lower[i]=w.value[i];
        s.upper[i]=w.value[i+32];
        if (active(w,i)) lo=wrap_add(lo,w.value[i]);
        if (active(w,i+32)) hi=wrap_add(hi,w.value[i+32]);
        s.lower_prefix[i]=lo;
        s.upper_prefix[i]=hi;
    }
    s.lower_total=lo;
    s.upper_total=hi;
    return s;
}
Read split_read(const Split& s,unsigned destination,unsigned source) {
    if (!(s.mask & bit(destination)) || !(s.mask & bit(source)))return {false,0};
    // Explicit shared-memory cross-half bridge.
    return {true,source<32?s.lower[source]:s.upper[source-32]};
}
uint32_t split_total(const Split& s) {
    return wrap_add(s.lower_total,s.upper_total);
}
uint32_t split_prefix(const Split& s,unsigned destination) {
    return destination<32?s.lower_prefix[destination]:
        wrap_add(s.lower_total,s.upper_prefix[destination-32]);
}
uint32_t naive_wave32_prefix(const Split& s,unsigned destination) {
    // Deliberate negative control: misses preceding physical wave32 sum.
    return destination<32?s.lower_prefix[destination]:s.upper_prefix[destination-32];
}
uint32_t next(uint32_t& state) {
    state^=state<<13;state^=state>>17;state^=state<<5;
    return state;
}
void require(bool yes,const std::string& problem) {
    if(!yes)throw std::runtime_error(problem);
}
void check(const Wave& w) {
    const Split s=split(w);
    require(split_total(s)==oracle_total(w),"cross-half total mismatch");
    require(s.mask==w.active,"ballot/active mask mismatch");
    for(unsigned dest=0;dest<64;dest++){
        if(!active(w,dest))continue; // inactive destination has no result
        require(split_prefix(s,dest)==oracle_prefix(w,dest),"prefix mismatch");
        for(unsigned src=0;src<64;src++){
            const Read expected=oracle_read(w,dest,src);
            const Read actual=split_read(s,dest,src);
            require(actual.defined==expected.defined,"undefined source mask mismatch");
            if(expected.defined)require(actual.value==expected.value,"cross-half lane read mismatch");
        }
        const unsigned xor_source=dest^32u;
        const Read cross=split_read(s,dest,xor_source);
        const Read oracle=oracle_read(w,dest,xor_source);
        require(cross.defined==oracle.defined,"xor cross-half mask mismatch");
        if(oracle.defined)require(cross.value==oracle.value,"xor cross-half value mismatch");
    }
}
} // namespace

int main() {
    try {
        Wave w{};
        w.active=~uint64_t(0);
        for(unsigned i=0;i<64;i++) w.value[i]=i+1;
        check(w);
        // A naive per-wave32 prefix is wrong at the first lane of the second half.
        require(naive_wave32_prefix(split(w),32)!=oracle_prefix(w,32),
                "negative control did not detect cross-half dependency");
        const uint64_t masks[] = {
            0,~uint64_t(0),bit(31)|bit(32),bit(0)|bit(63),
            0xAAAAAAAAAAAAAAAAull,0x5555555555555555ull,
            0xFFFFFFFFull,0xFFFFFFFF00000000ull,1ull,bit(63)
        };
        for(uint64_t mask:masks) { w.active=mask; check(w); }
        uint32_t rng=0x51504C3Du;
        for(unsigned run=0;run<320;run++){
            w.active=(uint64_t(next(rng))<<32)|next(rng);
            for(unsigned lane=0;lane<64;lane++)w.value[lane]=next(rng);
            check(w);
        }
        std::cout<<"wave64 CPU oracle vs split-wave32 bridge PASS (330 fixtures); "
                 <<"GPU_execution=false, game_tested=false\n";
    } catch(const std::exception& e){
        std::cerr<<"wave64 CPU fixture FAILED: "<<e.what()<<'\n';
        return 1;
    }
    return 0;
}
