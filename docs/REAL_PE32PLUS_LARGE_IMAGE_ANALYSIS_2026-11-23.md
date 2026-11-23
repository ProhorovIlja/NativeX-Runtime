# Large AMD64 PE32+ source inspection: real-world input gate

**Date:** 2026-11-23. **Scope:** original read-only PE32+ analysis and source
fixture regressions, not title compatibility, not a shipping game launcher,
not code execution. **No commercial executable or asset contents are stored
in this repository.**

## What a real-world input exposed

An independently obtained, user-provided 120,181,760-byte PE32+ AMD64
binary has 68 DLL import descriptors, 587 imported functions (24 by
ordinal), 299,257 AMD64 RUNTIME_FUNCTION entries, two TLS callback RVAs,
and 2,763 relocation blocks. The executable includes GameCore-specific
imports such as `xmem.dll`, `d3d12_xs.dll`, `chad.dll`,
`xg_xs.dll` and `XboxBvhBuilder.dll`. Source metadata also contains
multiple Xbox GRDK/GXDK SDK generations, including April 2026. These
values were observed directly by offline parsers and checked against
independent metadata readers. Their presence cannot authenticate the
binary, prove a package version, validate Windows ABI or prove it will boot.

The previous read-only `pe_intake` fixture parser rejected this image:
its protective exception-directory bound was 65,536 entries. Simply
increasing that cap and emitting all detailed unwind operations produced
a diagnostic JSON file around **70 MiB**, needlessly expensive for routine
research.

## Implemented code and measurable result

- `src/pe_intake.cpp`: optional `--compact` only for large images.
  It permits at most **1,000,000** `.pdata` entries, still validates each
  function range and each referenced AMD64 unwind header and operation,
  returns imports/exports (including IAT RVAs), TLS and relocations,
  but emits compact unwind version/flag/stack/count aggregates instead of
  a per-function JSON array. The original default mode preserves its
  stricter 65,536-entry bound and existing JSON schema expectations.
- `tests/check_large_pe_compact.py`: synthesizes a clean
  **65,537-entry** PE32+ exception table, checks that default mode rejects
  it, the compact mode produces small correct JSON, and malformed unwind
  references remain rejected.
- Verified manually with GNU C++17 `-Wall -Wextra -Werror -pedantic -O2`.
  The user-provided real PE produced **87,725 bytes** of JSON in
  **~0.27s / 148 MiB RSS** in a local Linux test environment, without
  running executable code. Numbers describe this read-only parser,
  **not gameplay performance**.
- Original synthetic PE fixture, the new large fixture, the
  PE import-to-binding/export/package pipeline, source-built Win64 DLL
  export/ordinal/forwarder fixture, and **23 Python unit tests** all passed
  locally. The source-built DLLs are *inspected*, never executed.

## Manual reproduction (no GitHub Actions)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pe_intake
ctest --test-dir build -R 'pe32plus_(readonly_fixture|large_pdata_compact_selftest|native_import_binding_plan_pipeline|sourcebuilt_host_dll_exports_and_forwarders)' --output-on-failure
python -m unittest discover -s tests -p 'test_pe_native_*.py' -v
```

A manually obtained authorized PE may be checked locally with
`./build/pe_intake /path/to/file.exe --compact`. **Never execute a
suspicious downloaded executable** to inspect its structure.

## Why the imported functions matter

The 587 imports are observations, not 587 solved native host calls.
Critical work remains in Xbox memory and physical-page mappings,
D3D12.X device/vtable/shaders, GPU texture tiling/BVH,
Xbox audio DSP and Bink video, GameInput ordinal `#1`,
and package-specific asset I/O. A coincidental upstream claim of
`587/587` retail imports does **not** prove it covers the identical
symbols, module versions or actual call signatures. `OpenScarlett` and
`Xbox2Windows` provide permissively licensed source-level research;
`xcl` has a GPL-2.0-only license and is not blindly combined with MIT.

The intended product remains a **native Windows x64 title executable**,
not a generalized Xbox emulator. No GTA VI entry point, RAGE world state,
real graphics frame, first `Present`, or FPS was tested here.
No user-origin binary was committed, redistributed or uploaded to GitHub.
No automation or schedule was created or started.
