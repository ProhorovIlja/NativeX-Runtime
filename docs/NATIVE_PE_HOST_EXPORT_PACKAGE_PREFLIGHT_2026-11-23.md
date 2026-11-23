# GTA VI native Windows: PE export and package-dependency research stage

**Date:** 23 November 2026  
**Repository:** https://github.com/ProhorovIlja/Scarlett-Windows-Compat  
**Branch:** `feat/native-pe-import-binding-dry-run-20261123`  
**Product goal:** Direct native Windows x64 executable (later Linux ELF), not a general Xbox emulator.

## Completed and independently run on this turn

1. Read-only C++17 PE32+ metadata inspector `src/pe_intake.cpp` now includes complete nonzero Export Address Table entries (`export_addresses`), including ordinal-only exports, the internal DLL export name, named exports and bounded forwarder strings. It rejects a named export with a zero EAT address, an out-of-image export target, a forwarder without a terminator in the declared export directory, and an IAT slot outside SizeOfImage.
2. New `tools/pe_native_export_verify.py` checks a native binding plan against explicitly supplied, read-only host PE export inventories. It follows bounded DLL-to-DLL forwarders by name or `#ordinal`, and refuses missing exports/modules, malformed forwarders, cycles, duplicate or inconsistent named/ordinal EAT entries, malformed JSON schemas and path-like module identities. The result always reports `abi_verified=false` and `game_executed=false`.
3. New `tools/pe_native_package_preflight.py` accepts a *supplied* multi-PE package inventory. It derives deterministic immediate-import dependency ordering, identifies explicit but unverified external DLL declarations, tracks delayed imports separately, reports orphan images, missing modules and cycles, and refuses to claim ABI or package execution.
4. Existing `tests/check_pe_intake.py` covers new export and IAT corruption cases. `tests/check_pe_binding_pipeline.py` now source-builds synthetic examples using the compiled inspector: import -> binding manifest -> direct and forwarded host export -> two-image package dependency graph. New tests `tests/test_pe_native_export_verify.py` and `tests/test_pe_native_package_preflight.py` cover the independent checkers.

**Local verification:** GNU C++17 `-O2 -Wall -Wextra -Werror -pedantic` build passed. Existing synthetic PE read-only parser tests passed. Expanded PE binding/export/package pipeline passed. **23 Python tests passed** (`python -m unittest discover -s tests -v`). All fixtures use generated synthetic PE files; no GTA VI or other commercial game image is involved. These runs were local, not Windows/MSVC CI or real game execution.

## What this result does NOT mean

- No compiled or relaunched GTA VI Windows executable, no actual game data inspection, no changes to console files.
- No native GDK/Xbox API implementation in this change; names of declared host exports do not prove calling convention, semantics, security or correctness.
- No real platform DLL load, native `GetProcAddress` resolution, IAT patch, PE relocation, DLL entry calls, GPU shader translation, audio/animation or gameplay FPS test.
- External module declarations are evidence of a *manual plan*, not proof that a real DLL exists on a particular Windows PC.
- The checks are strict and conservative: legitimate package DLL cycles may require more nuanced real-loader semantics. Here they remain unsupported rather than silently invented.

## Execution and scope

Inside the checked-out repository, invoke manually as needed:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pe_intake
ctest --test-dir build -R 'pe32plus_(readonly_fixture|native_import_binding_plan_pipeline)' --output-on-failure
python -m unittest discover -s tests -p 'test_pe_native_export_verify.py' -v
python -m unittest discover -s tests -p 'test_pe_native_package_preflight.py' -v
```

No auto-starting service, unattended file or package updates, scheduled tests, background monitoring or recurring tasks are introduced. Changes remain on a feature branch; merging to `main` or creating a pull request may trigger preexisting CI workflows and has **not** been performed.

## Upstream context examined

- [Microsoft PE/COFF documentation](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format): export address tables, ordinal indexes, and forwarder RVAs in the export data directory.
- [Xbox2Windows PR #930](https://github.com/R0Y00/Xbox2Windows/pull/930): authors report explicit package import preflight, dependency-ordered bundled DLL loading and refusal when an external provider is absent. This project only adopts the independent static *preflight test idea*, not the upstream runtime code.
- [Xbox2Windows PR #934](https://github.com/R0Y00/Xbox2Windows/pull/934): shared physical page backing with source-built Windows fixtures; a separate graphics/memory research issue.
- [OpenScarlett](https://github.com/californiaAlmonds/OpenScarlett) recent retail GDK imports/VM/graphics improvements were inspected as upstream claims, not reproduced for GTA VI.
- Both Xbox2Windows and OpenScarlett have MIT license files; XCL is GPL-2.0-only, hence its code is not merged into this MIT repository.

## Next correctness gates (not accomplished)

1. Windows x64 MSVC clean-build and CTest for the new sources on an explicitly permitted **manual** invocation only.
2. Source-built Windows-native *real DLL* export-versus-guest mapping differential, including named/ordinal exports and forwarders; verify actual PE and native ABI, not a synthetic guessed export map.
3. Implement only supported GDK-derived ABI adapters and loader import preflight for independently compiled test modules.
4. After obtaining legitimately accessible matching game executable and data, inspect real imports, multiple game DLLs, graphics features and storage archives; integrate actual startup while preserving the target directly launched `GTA6.exe`.
