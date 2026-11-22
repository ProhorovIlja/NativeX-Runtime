# PE32+ AMD64 .pdata / RUNTIME_FUNCTION metadata inventory

Original, read-only addition to `src/pe_intake.cpp` and the synthetic PE tests on 2026-11-22.

Windows x64 exception handling does **not** depend solely on an exported DLL symbol: the OS uses unwind metadata to walk non-leaf frames. The IMAGE_DIRECTORY_ENTRY_EXCEPTION (index **3**) points to an array of 12-byte AMD64 **RUNTIME_FUNCTION** entries containing BeginAddress, EndAddress and UnwindData relative virtual addresses.

## What is implemented

The tool now reports `runtime_functions` with numeric `begin_rva`, `end_rva` and `unwind_rva`. Its prior JSON import modules/named or ordinal imports, delay-loaded symbols, exports, TLS directory and relocation block counters are unchanged. The parser requires an integral 12-byte table with both RVA and size, at most **65,536** entries, a mapped contiguous on-disk section, a nonempty half-open function range within `SizeOfImage`, a nonzero aligned unwind-info RVA inside the image and ascending nonoverlapping function intervals. Arithmetic, file bounds and malformed RVAs are checked before collecting entries.

The fixture constructs two original synthetic entries `[0x1000,0x1020) → 0x1350` and `[0x1020,0x1040) → 0x1360`. Independent negative cases mutate the exception directory size/RVA, invalid begin/end, unaligned/out-of-image unwind addresses, overlapping intervals and out-of-image function ends, and demand a nonzero parser exit without executing or loading the PE.

`unwind_codes_decoded: false` is deliberate: this is **only metadata**. We do not parse x64 UNWIND_INFO versions/flags, chained unwind, handler scopes, stack restore instructions, C++ exceptions, SEH filters, shadow stacks or GDK-specific ABI conventions. A future actual title will require correctly interpreting this data and calling the real Windows unwinder, not inventing exception handlers.

## Useful source research

- [Microsoft x64 exception handling](https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64)
- [PE format exception data](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
- [RtlLookupFunctionEntry](https://learn.microsoft.com/en-us/windows/win32/api/winnt/nf-winnt-rtllookupfunctionentry) — native Windows host runtime table lookup.

The existing Windows source-built `scarlett_abi_fixture.dll` may also contain compiler-generated .pdata: it is inspected by `pe_intake` in CI. No Xbox binaries, DRM bypass, proprietary game data, console emulation, scheduled task or background monitoring is required.

## Port acceptance boundary

Being able to **list** a native AMD64 exception directory does not show that a manually mapped PE has registered its function table with `RtlAddFunctionTable`, supports unwinding, invokes TLS callbacks correctly or can run a GTA VI scene. Further independent Windows tests should trigger genuine SEH and frame unwinding, and only then compare against a source-built loader's behavior. The Xbox Game Core build must be lawfully obtained before real imports or unwind metadata can be inventoried.
