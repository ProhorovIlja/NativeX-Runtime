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

## Section and export inventory correctness (2026-11-24)

The inspector now checks declared header size, optional-header directory count, raw and virtual section extents and unambiguous region ownership. Read-only RVA spans must fit one declared region and its file-backed bytes. Memory-only IAT spans may also fit a virtual zero-fill tail. Inspected RVA directories require complete address/size pairs and mapped spans. Direct export addresses must refer to mapped regions; distinct names may share an EAT ordinal, including when the name count exceeds the slot count. Empty/duplicate names are rejected. Added JSON fields: size_of_image, size_of_headers, section_ranges_checked. This conservative metadata policy is not an emulation of all Windows loader behavior. Certificate tables and unconsumed directories remain outside this inspection.

Reference: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

## Source-linked client and provider integration (2026-11-24)

The existing optional LLVM fixture now cross-links three original Win64 DLLs and two actual EXEs against generated import libraries. It validates actual named imports, a NONAME ordinal import, named and ordinal forwarders, a DLL-to-DLL import, IAT metadata, missing-provider negative controls and dependency order. The forwarder-only client demonstrates why a module-only dependency graph can pass while symbol-level verification fails: a forwarded provider is not necessarily listed in the client's import descriptors. Always distinguish module closure from symbol presence and runtime ABI compatibility. Neither client nor DLL is executed. The existing CTest registration and missing-tool skip behavior remain unchanged.

## Explicit symbol dependency closure (2026-11-24)

Run `python tools/pe_native_symbol_closure.py --package inventory.json --require-all` on original or authorized read-only inventories. The checker combines descriptor graph reachability with exact name/ordinal export checks and adds providers reached through referenced forwarders. Their own imports are included, preserving startup versus delayed reachability. External declarations are not export evidence. Output is JSON on stdout; exit 2 means malformed input, exit 3 means requested startup or all-symbol gate failed. Existing package limits apply, with a 32768 aggregate evidence-row limit. No executable loading, live address binding, ABI proof, API-set resolution or Windows loader simulation. The cycle policy is conservative and only supplied DLLs serve as export providers.
