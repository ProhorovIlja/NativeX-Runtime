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
