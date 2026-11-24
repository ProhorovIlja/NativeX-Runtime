# Native PE32+ tools

These are original read-only diagnostics, not a loader. No inspected executable is executed or modified.

## Inspector

Build pe_intake from CMake. Use the path appropriate to your platform:

```sh
./build/pe_intake /path/to/authorized-sample.exe --compact > pe.json
```

Default mode provides per-record unwind detail with a 65,536-entry limit. Explicit compact mode validates up to 1,000,000 entries and returns aggregate counts instead of large arrays. The maximum source file size remains 512 MiB. Compact mode keeps imports, export metadata, TLS and relocation counts.

## Binding and provider analysis

The three Python CLI tools operate only on supplied JSON files:
- tools/pe_native_import_binding_plan.py: explicit import mappings, unsupported and unresolved symbols; --pe-json, --bindings, --require-complete.
- tools/pe_native_export_verify.py: verify named/ordinal host exports and bounded forwarding against supplied inventories; --plan, --host-inventories, --require-present.
- tools/pe_native_package_preflight.py: explicit multi-image module graph, immediate dependency ordering, delayed imports and missing/cyclic dependencies; --package, --require-immediate, --require-all.

Each supports --out and --help. No declared mapping or present export proves calling convention or behavioral compatibility. Package name coverage is not proof of provider existence or runtime execution.

## Reproduce locally

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pe_intake --config Release
ctest --test-dir build -C Release -R 'pe32plus_(readonly_fixture|large_pdata_compact_selftest|native_import_binding_plan_pipeline|sourcebuilt_host_dll_exports_and_forwarders)' --output-on-failure
python -m unittest discover -s tests -p 'test_pe_native_*.py' -v
```

The optional source-built Windows DLL check requires LLVM clang and lld-link and reports explicit skip code 77 if absent. It does not execute the DLL. Verification on 2026-11-24: 4 integration checks and 23 unit tests passed using original fixtures. No real application startup or Windows runtime compatibility is asserted.

## Dependency and relocation validation (2026-11-24)

The package preflight distinguishes startup-only reachability from the complete graph including nested delayed imports. Existing startup reachability fields retain their meaning. New fields: `all_reachable_images`, `delayed_only_images`, `unreferenced_images`, and `delayed_reachable_immediate_cycles`. Missing optional-branch dependencies and immediate cycles in delayed-only modules cause `--require-all` to fail; startup-only success is unaffected. Graph ordering is a conservative diagnostic, not a Windows loader simulation.

The PE inspector reports relocation block and slot counts, ABSOLUTE padding, DIR64, unsupported raw types and per-type counts. It validates directory ranges, page and block alignment, and DIR64 target ranges within declared image sections or headers. Unknown types are counted, not interpreted. No relocation is applied; no image is mapped or executed. See the Microsoft PE/COFF specification for field semantics. These are structural checks, not ABI or runtime compatibility proof.
