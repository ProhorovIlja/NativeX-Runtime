// Synthetic CPUID/XCR0 dispatch testing; no title code or CPU benchmarking.
#include "native_cpu_dispatch.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace scarlett::cpu;
namespace {
constexpr std::uint32_t bit(unsigned value){return std::uint32_t{1}<<value;}
void check(bool ok,const char* reason) {
    if(!ok)throw std::runtime_error(reason);
}
Probe all_bits() {
    Probe p{};
    p.x64=true;p.max_basic_leaf=7;
    p.leaf1.edx=bit(26); // SSE2
    p.leaf1.ecx=bit(20)|bit(26)|bit(27)|bit(28)|bit(12);
    p.leaf7.ebx=bit(5)|bit(8)|bit(16);
    p.xcr0_readable=true;p.xcr0=0xe6u;
    return p;
}
void baseline_without_guesswork() {
    const auto empty=decode({});
    check(!empty.sse2 && !empty.sse42 && !empty.bmi2 &&
          !empty.avx && !empty.avx2 && !empty.fma &&
          !empty.avx512f && choose(empty)==Path::Scalar,
          "no CPUID data must not infer a modern CPU");
    auto p=all_bits();p.x64=false;
    check(choose(decode(p))==Path::Scalar,"non-x64 host accepted AVX kernels");
    p=all_bits();p.max_basic_leaf=0;
    check(choose(decode(p))==Path::Scalar,"leaf 1 absent but features claimed");
}
void independent_sse42_and_bmi2() {
    auto p=all_bits();
    p.xcr0_readable=false;
    const auto cap=decode(p);
    check(cap.sse2 && cap.sse42 && cap.bmi2 &&
          !cap.avx && !cap.avx2 && !cap.fma && !cap.avx512f &&
          choose(cap)==Path::Sse42,
          "SSE4.2 or BMI2 incorrectly tied to AVX OS state");
    p.leaf1.ecx &= ~bit(20);
    check(choose(decode(p))==Path::Scalar,"SSE4.2 kernel selected without SSE4.2");
    p=all_bits();p.max_basic_leaf=1;
    const auto old=decode(p);
    check(old.sse2 && old.sse42 && old.avx &&
          !old.avx2 && !old.bmi2 && !old.avx512f,
          "leaf 7 fabricated on old CPU");
}
void no_avx_without_xsave_osxsave_or_ymm_state(){
    auto p=all_bits();
    p.leaf1.ecx &= ~bit(26);
    check(!decode(p).avx && !decode(p).avx2,
          "AVX selected without CPUID XSAVE");
    p=all_bits();p.leaf1.ecx &= ~bit(27);
    check(!decode(p).avx && !decode(p).fma,
          "AVX selected without CPUID OSXSAVE");
    p=all_bits();p.leaf1.ecx &= ~bit(28);
    check(!decode(p).avx && !decode(p).avx2,
          "AVX selected without CPU AVX bit");
    p=all_bits();p.xcr0=0x2u;
    check(!decode(p).avx && !decode(p).avx2,
          "AVX selected when OS doesn't save YMM state");
    p=all_bits();p.xcr0=0x4u;
    check(!decode(p).avx && !decode(p).fma,
          "AVX selected when OS doesn't save XMM state");
}
void avx2_fma_and_avx512_are_separate() {
    auto p=all_bits();p.xcr0=0x6u;
    const auto avx=decode(p);
    check(avx.avx && avx.avx2 && avx.fma && !avx.avx512f &&
          choose(avx)==Path::Avx2 &&
          choose(avx,false)==Path::Sse42,
          "AVX2 available with XMM+YMM but not AVX512 state");
    p.leaf1.ecx &= ~bit(12);
    check(!decode(p).fma && decode(p).avx2,
          "FMA feature incorrectly assumed for AVX2");
    p=all_bits();
    const auto zmm=decode(p);
    check(zmm.avx512f && choose(zmm)==Path::Avx2,
          "AVX512F reporting should not auto-dispatch high-power kernels");
    p.leaf7.ebx &= ~bit(16);
    check(!decode(p).avx512f && decode(p).avx2,
          "AVX512 claimed without CPU flag");
    p=all_bits();p.leaf7.ebx &= ~bit(5);
    check(!decode(p).avx2 && decode(p).avx && decode(p).bmi2,
          "AVX2 incorrectly assumed from base AVX");
}
void actual_host_probe_never_executes_optimized_kernel(){
    const auto raw=inspect_host();
    const auto decoded=decode(raw);
    const auto selected=choose(decoded);
    check((selected==Path::Scalar || selected==Path::Sse42 ||
           selected==Path::Avx2) &&
          (!decoded.avx2 || decoded.avx) &&
          (!decoded.avx512f || decoded.avx2 || decoded.avx),
          "host probe reported impossible prerequisites");
    check(name(Path::Scalar)[0]=='s' &&
          name(Path::Sse42)[0]=='s' &&
          name(Path::Avx2)[0]=='a',
          "dispatch path identifiers changed");
    if(raw.xcr0_readable)
        check(raw.x64 && raw.max_basic_leaf>=1 &&
              (raw.leaf1.ecx&(bit(26)|bit(27)))==(bit(26)|bit(27)),
              "XGETBV executed without XSAVE and OSXSAVE");
    std::cout<<"native_host_probe: x64="<<(raw.x64?"true":"false")
             <<" sse42="<<(decoded.sse42?"true":"false")
             <<" avx2="<<(decoded.avx2?"true":"false")
             <<" avx512f="<<(decoded.avx512f?"true":"false")
             <<" dispatch_candidate="<<name(selected)<<'\n';
}
}
int main(){
    try{
        baseline_without_guesswork();
        independent_sse42_and_bmi2();
        no_avx_without_xsave_osxsave_or_ymm_state();
        avx2_fma_and_avx512_are_separate();
        actual_host_probe_never_executes_optimized_kernel();
    }catch(const std::exception& error){
        std::cerr<<"native CPU ISA dispatch FAIL: "<<error.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"x64-cpuid-isa-safe-dispatch\","
             "\"scenarios\":5,\"cpuid_xsave_xcr0_checked\":true,"
             "\"optimized_kernel_executed\":false,"
             "\"game_executable_used\":false,"
             "\"minimum_cpu_inferred\":false}\n";
    return 0;
}
