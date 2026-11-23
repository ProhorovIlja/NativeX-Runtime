// Native x86-64 instruction-set capability and safe dispatch policy.
// No emulation, CPU model whitelist, background service or game executable.
#pragma once
#include <cstdint>

namespace scarlett::cpu {
struct Registers {
    std::uint32_t eax=0,ebx=0,ecx=0,edx=0;
};
// Raw CPUID/XCR0 observations. Synthetic tests can supply arbitrary cases.
// Never infer that the operating system saves AVX register state from CPUID
// feature bits alone.
struct Probe {
    bool x64=false;
    std::uint32_t max_basic_leaf=0;
    Registers leaf1{};
    Registers leaf7{};
    bool xcr0_readable=false;
    std::uint64_t xcr0=0;
};
struct Capabilities {
    bool sse2=false;
    bool sse42=false;
    bool bmi2=false;
    bool avx=false;
    bool avx2=false;
    bool fma=false;
    bool avx512f=false; // reported only; never automatically dispatched
};
enum class Path : std::uint8_t { Scalar=0,Sse42=1,Avx2=2 };

// Pure, allocation-free and fail-closed, for independent synthetic validation.
Capabilities decode(const Probe& raw) noexcept;
Path choose(const Capabilities& features,bool allow_avx2=true) noexcept;
const char* name(Path path) noexcept;

// Real native-host probe. XGETBV(0) is executed only when BOTH CPUID XSAVE
// and OSXSAVE are reported; on non-x64 builds it returns an empty Probe.
// Call before opting into any compiler-generated advanced-ISA kernel.
Probe inspect_host() noexcept;
} // namespace scarlett::cpu
