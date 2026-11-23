# Native x86-64 CPU capability gates for Windows/Linux — 2026-11-23

## Purpose

The proposed GTA VI CPU requirement of Ryzen 5 5600 for 30 FPS is **unverified**; the opposite claim that any older CPU will necessarily reach 60 FPS is also unverified. Our native host must not hardcode either assumption. This original C++17 module identifies **instruction-set eligibility only**, preserving safe scalar fallbacks and allowing future verified kernels to select an instruction set without CPU-model whitelists.

This is **not Xbox CPU emulation**. The baseline host does not execute game code or translate CPU instructions. A SIMD path is only a candidate for future checked host algorithms. CPU vendor, clock speed, FPS, frame time or GTA VI minimum cannot be inferred from these feature bits.

## Implemented

- A pure, allocation-free decode(Probe) turns a synthetic or real CPUID leaf1/leaf7 snapshot and optional XCR0 into SSE2, SSE4.2, BMI2, AVX, AVX2, FMA and AVX512F capabilities.
- AVX, AVX2 and FMA are conditioned on CPU XSAVE + OSXSAVE + AVX flags and OS XCR0 **XMM+YMM mask 0x6**. AVX2 additionally requires leaf7 EBX bit5. FMA separately requires leaf1 ECX bit12. BMI2 requires leaf7 EBX bit8 and is **not** incorrectly gated by YMM state.
- AVX512F also requires the AVX CPU/OS gates, leaf7 EBX bit16 and XCR0 **0xE6** for mask/ZMM register state. This module reports AVX512F but **never automatically selects an AVX512 path**: workload performance and power behavior vary.
- choose(capabilities,allow_avx2) returns Scalar / SSE4.2 / AVX2. The caller may disable AVX2 for measured energy/latency trade-offs while retaining scalar fallback.
- inspect_host uses MSVC _cpuidex / _xgetbv on Windows x64 or GCC __get_cpuid_max / __cpuid_count and xgetbv on native Linux x64. Critically, xgetbv is executed **only after** CPUID reports XSAVE+OSXSAVE. Non-x64 targets return a neutral probe.
- Compiled without forcing a high ISA target (/arch:AVX2 or -mavx2); loading this base code does not require AVX. No extra threads, polling or emulated CPU instructions.

## Source-built acceptance

Five deterministic Windows/Linux test groups exercise missing CPUID leaves; old/no-AVX hosts; SSE4.2 and BMI2 independent of OS YMM state; missing XSAVE/OSXSAVE/AVX bits; individual XCR0 XMM/YMM failure; FMA/AVX2/AVX512F separate bits; explicit AVX2 disable; and native host probe without executing advanced SIMD kernels.

CI runner capability means **only that the CI host supports the reported instructions**. It is not a GTA VI benchmark. Future optimized memory/streaming kernels require exact-output scalar oracles and benchmarks on physical older/newer CPUs. CPU simulation, traffic, scripting and GPU bottlenecks will need end-to-end game instrumentation; rewrite to Rust alone cannot guarantee speedup.

## Primary documentation

- [Intel ISA extension programming reference: OSXSAVE and XGETBV](https://www.intel.com/content/dam/develop/external/us/en/documents/319433-024-697869.pdf).
- [Microsoft IsProcessorFeaturePresent](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-isprocessorfeaturepresent).
- [Microsoft GetEnabledXStateFeatures](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-getenabledxstatefeatures).

No game executable or assets, CPU emulation, OS drivers, scheduled tasks, recurring checks, monitoring, title FPS claims or proprietary SDK code were added.
