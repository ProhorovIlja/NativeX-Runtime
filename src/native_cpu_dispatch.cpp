#include "native_cpu_dispatch.hpp"
#if defined(_M_X64) || defined(__x86_64__)
#define SCARLETT_X64_HOST 1
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#else
#define SCARLETT_X64_HOST 0
#endif

namespace scarlett::cpu {
namespace {
constexpr bool has(std::uint32_t bits,unsigned bit) noexcept {
    return (bits&(std::uint32_t{1}<<bit))!=0;
}
}
Capabilities decode(const Probe& raw) noexcept {
    Capabilities out{};
    if(!raw.x64 || raw.max_basic_leaf<1) return out;
    out.sse2=has(raw.leaf1.edx,26);
    out.sse42=has(raw.leaf1.ecx,20);
    const bool xsave=has(raw.leaf1.ecx,26);
    const bool osxsave=has(raw.leaf1.ecx,27);
    const bool avx_cpu=has(raw.leaf1.ecx,28);
    const bool ymm_state=raw.xcr0_readable && xsave && osxsave &&
        (raw.xcr0&0x6u)==0x6u;
    out.avx=avx_cpu && ymm_state;
    out.fma=out.avx && has(raw.leaf1.ecx,12);
    if(raw.max_basic_leaf>=7) {
        out.bmi2=has(raw.leaf7.ebx,8); // BMI2 uses general-purpose registers.
        out.avx2=out.avx && has(raw.leaf7.ebx,5);
        const bool zmm_state=out.avx && (raw.xcr0&0xe6u)==0xe6u;
        out.avx512f=zmm_state && has(raw.leaf7.ebx,16);
    }
    return out;
}
Path choose(const Capabilities& features,bool allow_avx2) noexcept {
    // This chooses only a candidate code path; no AVX2 instructions are
    // emitted by this baseline-compiled library. Each optimized kernel must
    // still have a separate validated target and scalar fallback.
    if(features.avx2 && allow_avx2)return Path::Avx2;
    if(features.sse2 && features.sse42)return Path::Sse42;
    return Path::Scalar;
}
const char* name(Path path) noexcept {
    switch(path){
        case Path::Scalar:return "scalar";
        case Path::Sse42:return "sse4.2";
        case Path::Avx2:return "avx2";
    }
    return "unknown";
}
Probe inspect_host() noexcept {
    Probe out{};
#if SCARLETT_X64_HOST
    out.x64=true;
#if defined(_MSC_VER)
    int regs[4]{};
    __cpuidex(regs,0,0);
    out.max_basic_leaf=static_cast<std::uint32_t>(regs[0]);
    if(out.max_basic_leaf>=1){
        __cpuidex(regs,1,0);
        out.leaf1={
            static_cast<std::uint32_t>(regs[0]),
            static_cast<std::uint32_t>(regs[1]),
            static_cast<std::uint32_t>(regs[2]),
            static_cast<std::uint32_t>(regs[3])};
    }
    if(out.max_basic_leaf>=7){
        __cpuidex(regs,7,0);
        out.leaf7={
            static_cast<std::uint32_t>(regs[0]),
            static_cast<std::uint32_t>(regs[1]),
            static_cast<std::uint32_t>(regs[2]),
            static_cast<std::uint32_t>(regs[3])};
    }
#else
    out.max_basic_leaf=__get_cpuid_max(0,nullptr);
    if(out.max_basic_leaf>=1)
        __cpuid_count(1,0,out.leaf1.eax,out.leaf1.ebx,
                         out.leaf1.ecx,out.leaf1.edx);
    if(out.max_basic_leaf>=7)
        __cpuid_count(7,0,out.leaf7.eax,out.leaf7.ebx,
                         out.leaf7.ecx,out.leaf7.edx);
#endif
    constexpr std::uint32_t Xsave=std::uint32_t{1}<<26;
    constexpr std::uint32_t Osxsave=std::uint32_t{1}<<27;
    if(out.max_basic_leaf>=1 &&
       (out.leaf1.ecx&(Xsave|Osxsave))==(Xsave|Osxsave)){
#if defined(_MSC_VER)
        out.xcr0=_xgetbv(0);
#else
        std::uint32_t lo=0,hi=0;
        __asm__ __volatile__("xgetbv" : "=a"(lo),"=d"(hi) : "c"(0));
        out.xcr0=(static_cast<std::uint64_t>(hi)<<32)|lo;
#endif
        out.xcr0_readable=true;
    }
#endif
    return out;
}
} // namespace scarlett::cpu
