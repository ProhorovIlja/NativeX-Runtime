# Current development state — 2026-11-24

## Public runtime consolidation

- Project identity: NativeX Runtime, formerly Scarlett Windows Compat. Existing CMake target names and third-party licenses remain intact.
- Incorporated the 15 independently validated PE source/tool/test files from [the prior feature revision](https://github.com/ProhorovIlja/NativeX-Runtime/tree/005b5923cd79a0a264ab6191e46c399804346cac) by exact blob identity. Added the existing fixture registration to CMake and updated its project name.
- Complete ordinal-only export inventory, bounded forwarder checking, explicit binding-plan verification and package dependency ordering.
- Opt-in compact PE metadata mode verifies up to 1,000,000 exception records under the existing 512 MiB file bound; original detailed mode retains the lower limit.
- Local verification on 2026-11-24: strict GNU C++17 build; original PE fixture, large .pdata fixture, end-to-end PE binding/export/dependency pipeline and source-built LLVM-linked Win64 DLL forwarder fixture all passed. **23/23 Python unit tests passed.** No proprietary input was used for this run; no Windows DLL was executed by the cross-build fixture.
- Earlier merged Windows/Linux checks cover synthetic D3D12/WARP, Vulkan, BC texture and native host functionality. See the dated engineering reports for exact conditions and limitations. The last previous successful main CI was [45199939980](https://github.com/ProhorovIlja/NativeX-Runtime/actions/runs/45199939980); it does not certify the new revision.
- No fresh Windows/MSVC CI, physical GPU performance, complete console ABI, or commercial-title startup is claimed. Automatic Actions were disabled by the repository owner; no new scheduled tasks or runs requested.

Prior documentation and chronological engineering history are preserved at [the original main revision](https://github.com/ProhorovIlja/NativeX-Runtime/tree/14a8806993bcd73887c63c5261ae4d62598f759b). See [PUBLIC_SCOPE.md](PUBLIC_SCOPE.md) and [NATIVE_PE_TOOLS.md](NATIVE_PE_TOOLS.md) for the current public scope.

## PE correctness follow-up — 2026-11-24

- Fixed full dependency closure through nested delay-loaded modules; four additional regression tests cover delayed missing dependencies, optional immediate cycles, delay-only cycles and precedence of startup paths.
- Added read-only DIR64 target bounds and base relocation slot/type inventory, with malformed-range negative controls and unsupported-type reporting.
- Strict GNU C++17 build and four standalone PE checks passed, with 27/27 Python unit tests. Original synthetic fixtures and source-built DLL metadata only. No Windows execution or new CI runs; no proprietary inputs.

## Section/RVA/export correctness follow-up — 2026-11-24

- Added bounded header and section extent validation, conservative overlap rejection, consistent mapped RVA ranges, complete inspected data directory pairs, mapped IAT and export targets, export ordinal aliases and duplicate-name rejection.
- 17 new malformed-layout controls and two positive controls (aliases and virtual IAT tail) passed; strict GNU C++17 compilation, all four standalone integration checks, and 27/27 Python unit tests passed. The new cases are inside the original PE integration script. No proprietary inputs or Windows executable execution. No fresh MSVC or complete repository build.

## Source-linked PE validation follow-up — 2026-11-24

- Extended the independent host-export fixture with three linked DLLs and two real client EXEs using Clang/LLD 17 and original C source. Five actual client import identities, direct and forwarded named/ordinal exports, bridge dependency order, missing-forwarder provider controls and no modification of client files verified.
- Strict GNU C++17 build, 27/27 Python unit tests and four integration checks passed. Cross-linked PE metadata only; no Windows execution, complete repository CMake build, ABI proof, or application startup. No proprietary input or upstream code copied. No automatic jobs requested.

## Symbol dependency closure follow-up — 2026-11-24

- New original read-only combined package/symbol closure checker, including forwarder-induced provider imports, missing symbol evidence and startup/delayed reachability. No runtime execution or proprietary material.
- 40/40 Python tests (13 new) and all four PE integration checks passed, including source-linked Win64 fixtures. Strict GCC C++17 build passed. This validates metadata, not ABI or application startup. No automatic jobs requested.

## Bounded asset streaming follow-up — 2026-11-24

- Added configurable requested-byte backpressure for queued and executing reads and synchronized load statistics. Existing weighted queue and read semantics preserved, with earlier cancellation admission and post-hash check.
- Original streaming test passed on strict GCC 14.2; new concurrent admission test passed on GCC, Clang 17, and GCC ASan/UBSan. Each run made 512 attempts with a 128 KiB test cap; peak stayed at 131072 bytes. No Windows or full CMake build, DirectStorage/io_uring, real-world throughput or proprietary input tested. New CTest registration added for future manual execution; no scheduled jobs requested.

## Critical stream admission follow-up — 2026-11-24

- Added opt-in critical byte and queue-slot reservations, noncritical credit tracking, and synchronized statistics. Default zero reservation preserves previous behavior. Existing reader and budget fixtures passed.
- Strict GCC/Clang and GCC ASan/UBSan: three 1536-attempt mixed-priority runs, 262144-byte cap, 131072-byte reserve, two reserved slots. No sampled invariant violations; accepted bytes compared against independent data. No Windows or full CMake build, application startup, SSD or GPU performance verification. New manual test registration only; no automation requested.

## Completed asset cache follow-up — 2026-11-24

- Added independent bounded LRU cache for completed native asset reads with generation-aware keys and explicit insertion. Reader unchanged. Strict GCC and Clang builds and GCC ASan/UBSan passed; each cache test run avoided 1008 of 1024 Reader submissions in a repetitive synthetic workload and observed no invariant failures across 4096 concurrent operations. Existing three stream fixtures passed on GCC. No proprietary input, Windows execution, full CMake build, real game, SSD timing or GPU test. New manual CTest registration only.

## Single-flight asset reads follow-up — 2026-11-24

- Added original bounded caller-side request coalescing with immutable shared cache publication, priority separation, cancellation isolation, failure retry and exception fan-out. Strict GCC and Clang and GCC ASan/UBSan passed; all four existing stream/cache fixtures passed. Each new test run combined 16 controlled simultaneous callers into one backend invocation and 1024 real Reader logical requests into two submissions. Windows execution, full CMake build, DirectStorage, GPU and real game performance remain unverified. No proprietary inputs or upstream code copied. Manual CTest registration only.

## Published payload lifetime budget follow-up — 2026-11-24

- Implemented optional per-cache immutable payload accounting through final consumer release, including evicted and uncached data. Backpressure and retry are integrated with the coalescing adapter. Five prior fixtures rebuilt and passed on GCC.
- Strict GCC and Clang and GCC ASan/UBSan passed three 4096-operation concurrent tests, each at 1024-byte peak with zero invariant errors and zero final live bytes. No proprietary inputs, upstream code import, Windows execution, full CMake build, GPU or game performance verification. Manual CTest registration only.

## Pre-I/O asset commitment follow-up — 2026-11-24

- Added move-only pre-read reservations and exact live/reserved credit transfer. Six previous fixtures passed strict GCC C++17; new fixture passed GCC 14.2, Clang 17 and GCC ASan/UBSan, three 4096-attempt runs at a 1024-byte cap with zero observed invariant errors and zero final committed bytes. Controlled in-flight denial avoided backend I/O. Prior live-budget test now expects two actual backend calls rather than three.
- Original code and synthetic fixtures only. No proprietary input, full CMake or Windows execution, real SSD timing, GPU or game performance verification. Manual CTest registration only.

## Bounded Zstandard decoder follow-up — 2026-11-24

- Implemented optional exact-length bounded single-frame Zstandard adapter using installed 1.5.7. Original fixture tests malformed, truncated, excess output, small window, empty, checksum, trailing/concatenated frames, pre-cancel and actual Reader/CoalescedReader integration. Strict GCC 14.2, Clang 17 and GCC ASan/UBSan passed 128 parallel decodes each, zero errors. Sanitizers instrument wrapper, not installed system library. Seven unchanged previous streaming binaries re-run and passed. Full CMake/Windows builds, game codec identification, actual game data and performance remain unverified. Manual CTest registration only; no automatic jobs.

## Shared decode budget follow-up — 2026-11-24

- Added original BudgetedDecoder and end-to-end independent CPU pixel assembly fixture. Strict GCC and Clang and GCC ASan/UBSan passed 1024 concurrent attempts each, 196608-byte peak, max two active decodes, zero observed errors and final committed bytes. All 16384 pixels matched. All eight prior stream/cache/decode fixtures rebuilt and passed. System Zstd not sanitizer-instrumented. No game data, new upstream code, GPU or Windows execution. Exact vendored bcdec header could not be transferred to local environment, so BCn integration not tested. New optional manual CTest registration; no automation.

## Portable texture upload staging follow-up — 2026-11-24

- Added independent CPU texture staging with mip/array layout, BC block geometry, aligned zero padding, cap and source validation. GCC and Clang strict C++17 and GCC ASan/UBSan passed both staging and extended decode-budget fixtures (six executions). Integrated 16384 CPU pixels and all staged bytes matched; 3072 concurrent decode attempts across three runs, no budget invariant errors. System Zstd not sanitizer-instrumented. No actual GPU, Windows, complete CMake build, BC decoding or game execution tested. PortWatch source revision unchanged; Sonoma pipeline source reviewed, not imported. Manual CTest registration only; no automation.
