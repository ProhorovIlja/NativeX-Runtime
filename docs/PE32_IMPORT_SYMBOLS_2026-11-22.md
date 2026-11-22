# Read-only native PE32+ import-symbol inventory (2026-11-22)

Original C++17 parser `src/pe_intake.cpp` has been extended to inspect **IMAGE_THUNK_DATA64** lookup tables for ordinary and delay-load import descriptors. It does not map or execute a PE, modify a binary, resolve functions, inject modules, bypass signatures, install hooks, or rely on game files.

## Exact output

Previously `pe_intake` listed only imported DLL names. New JSON arrays:

- `import_symbols` — each static PE32+ module's imported symbol, either an ASCII `name` with `ordinal: null` or a numeric 16-bit `ordinal` with `name: null`.
- `delay_import_symbols` — same structure for `IMAGE_DELAYLOAD_DESCRIPTOR` import-name tables.
- `import_modules` and `delay_import_modules` are retained unchanged for compatibility with existing inspection scripts.

The tool first reads `OriginalFirstThunk` (lookup table), falling back to `FirstThunk` when lookup metadata is missing; delay import descriptors use `pINT` then `pIAT`. Each 64-bit thunk is read through the existing bounds-checked RVA→raw-section mapping. The parser recognizes the high ordinal flag and validates that no unrelated bits are present, otherwise checks the 32-bit PE import-by-name RVA, skips its 2-byte hint, and reads a bounded ASCII NUL-terminated symbol. All section offsets, truncated strings and invalid thunks fail closed; at most **8,192 symbols per import table and 32,768 per import class** can be accepted before an error.

## Independent synthetic acceptance

The enlarged original on-disk PE32+ test includes `KERNEL32.dll!GetCurrentProcessId` by name, ordinal **#123** and a delayed `USER32.dll!MessageBoxW` by name, with valid zero terminations. It also preserves existing export names/ordinals/forwarder checks, relocation block, TLS directory and intentional negative corruptions. Additional negative fixtures change import-by-name RVAs to invalid addresses, inject illegal high bits into 64-bit thunks and corrupt delay-load INT addresses; **parsing must reject them without code execution**.

An existing separate Windows CI checks the parser against the separately **source-built** `scarlett_abi_fixture.dll`; runtime linking or full Windows loader compatibility cannot be inferred from read-only analysis.

This enables a future lawful audit of actual Xbox PE/GDK imports to identify which dependencies need explicit Windows x64 ABI adapters. It does **not** claim knowledge of which APIs GTA VI will import, what an Xbox GDK binary truly looks like, whether GPU D3D12.X calls translate, or whether an Xbox title can start on PC.

## Follow-up

Once legally obtained game files are available, produce a reproducible manifest grouped by imported module and symbol/ordinal, delay-load status and unresolved ABI adapter. Compare with the **documented** Microsoft GDK contracts and independently source-validated compatibility providers. Reject undocumented guessed function signatures. Preserve exact original build identifiers without publishing proprietary binaries. Windows native execution remains priority 1, Linux ELF native target priority 2 and multiplayer later.
