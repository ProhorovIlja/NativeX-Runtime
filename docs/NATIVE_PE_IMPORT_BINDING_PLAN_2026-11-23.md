# Native PE32+ import binding plan — 2026-11-23

**Scope:** native Windows x64 port bring-up, before any authorized game executable is available. This is a source-built offline analysis tool, **not** an Xbox emulator, Xbox executable loader, IAT patcher, or proof that GTA VI boots.

## What is actually implemented

- The existing read-only `pe_intake` now emits **64-bit PE import lookup thunk RVAs**, **IAT slot RVAs** and name-table **hints**, for normal and delay-load import tables. Missing FirstThunk produces `null` IAT fields, never a guessed writable address.
- `tools/pe_native_import_binding_plan.py` consumes the metadata plus a user-authored, versioned `scarlett-native-import-bindings/v1` manifest and deterministically classifies symbols as explicit `host-export`, `native-adapter`, `unsupported` or `unresolved`. DLL identity is compared case-insensitively; function names remain case-sensitive; ordinals are distinct from names.
- Every previously unknown import remains `unresolved`, even if its DLL name resembles a Windows system DLL. The tool does **not** call LoadLibrary, resolve host function pointers, rewrite any binary or execute any analyzed executable. No compatibility mapping for the actual GTA VI executable is claimed.
- Input JSON is bounded to 2 MiB/file; at most 32,768 imports and declarations are handled. Ambiguous records, duplicate manifest keys, misaligned IAT/lookup slots, unsupported actions and invalid metadata fail closed. Optional JSON output uses temporary-file replacement after successful validation, preserving prior reports if input validation fails.
- `--require-complete` returns exit code 3 if **any** import is unsupported or unresolved, while still emitting the report. Exit code 2 denotes malformed input. No absence of missing imports is inferred when data is unavailable.

## Large PE32+ executable inspection (source-only)

When examining a large, legitimately accessible AMD64 executable, explicitly opt
in to bounded compact metadata (max 1,000,000 validated runtime-function
records, 512 MiB source-file budget):

```sh
./build/pe_intake /path/to/authorized.exe --compact > inspect.json
python tools/pe_native_import_binding_plan.py --pe-json inspect.json \\
  --bindings explicit_host_adapters.json --out import_plan.json
```

In `--compact`, the parser retains import/export symbols, IAT RVAs, TLS
callback inventory, and relocation block count, but omits the huge raw
`runtime_functions` and `unwind_infos` arrays and publishes aggregate
counts. All recorded unwind ranges and version-one operations are still
checked. The default one-argument mode retains the original **65,536**
runtime-function limit and unchanged full detail for smaller synthetic tests.

A filled import binding manifest is a *candidate plan*, never proof of ABI
compatibility, executable authenticity, malware safety or game startup.
Do not commit original commercial executable bytes or its raw proprietary
sections to GitHub.

## How to reproduce with legal synthetic inputs

On Linux or Windows, from repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pe_intake
ctest --test-dir build -R 'pe32plus_(readonly_fixture|native_import_binding_plan_pipeline)' --output-on-failure
python -m unittest discover -s tests -p 'test_pe_native_import_binding_plan.py' -v
```

The existing `tests/check_pe_intake.py` generates an original synthetic PE32+ including named imports, an ordinal and a delay-loaded import; `tests/check_pe_binding_pipeline.py` feeds its generated file to the compiled PE inspector and verifies the resulting explicit binding plan. Neither test requires game assets, network access or proprietary SDK binaries.

Example explicit manifest entry:

```json
{
  "schema": "scarlett-native-import-bindings/v1",
  "bindings": [
    {
      "guest_module": "KERNEL32.dll",
      "guest_name": "GetCurrentProcessId",
      "action": "host-export",
      "host_module": "kernel32.dll",
      "host_symbol": "GetCurrentProcessId"
    }
  ]
}
```

Use it with an existing JSON output of `pe_intake`:

```sh
python tools/pe_native_import_binding_plan.py --pe-json inspect.json --bindings bindings.json --out native-import-plan.json --require-complete
```

A `host-export` declaration means *candidate binding*, not proven ABI compatibility. Actual adapter semantics, import handling, PE relocation/startup, CPU instructions, GDK, D3D12.X, shaders, protected data and executable correctness still need independent validation.

## Local verification / branch status

- Locally built patched PE inspector with GNU C++17 `-Wall -Wextra -Werror -pedantic -O2`.
- Existing synthetic PE inspector tests: **passed**, including new IAT slot checks, overflow and absent FirstThunk cases.
- New native-binding planner Python unit tests: **6/6 passed**.
- The new PE-file-to-binding-plan pipeline test: **passed** against the compiled inspector and generated synthetic PE.
- Changes were committed only to `feat/native-pe-import-binding-dry-run-20261123`, not merged into `main`. **No GitHub Actions execution is claimed or requested**, and no scheduled workflow/monitor was created or modified.

## Next integration gate

For any legally available actual Xbox executable: inspect the *real* import/delay-import modules and IAT offsets, compare GDK export names and call ABI, implement tested native Windows functions and shader/resource translations, and only then create host-native startup. No Xbox/GTA VI binary is provided or analyzed in this change.
