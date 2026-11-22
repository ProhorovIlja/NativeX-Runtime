# Read-only PE32+ IMAGE_TLS_DIRECTORY64 and callback address inventory (2026-11-22)

Extends the original C++17 `pe_intake` CLI to extract **TLS metadata**, an essential future native Windows Xbox Game Core loader-compatibility gate. The exact source is `src/pe_intake.cpp`; the same independently constructed synthetic fixture used for imports, .pdata, relocations and exports now includes a populated x64 TLS directory.

## Fields parsed from the PE

The PE optional header's **TLS directory index 9** points by an RVA to a 40-byte `IMAGE_TLS_DIRECTORY64` data structure. Its contained `StartAddressOfRawData`, `EndAddressOfRawData`, `AddressOfIndex` and `AddressOfCallBacks` fields are **absolute 64-bit image virtual addresses**, not RVAs. The parser subtracts the ImageBase only after verifying each address is inside `SizeOfImage`. It reads the TLS zero-fill count, requires sensible template endpoints, validates the four-byte TLS index location and scans at most **256** NUL-terminated 64-bit callback pointers through the already bounds-checked file RVA mapper. Invalid file bytes, overlarge zero fill, invalid callback VA, unbounded callback arrays and inconsistent start/end cause a nonzero exit.

Structured JSON fields:

- `has_tls_directory`: whether a valid TLS directory was located.
- `tls_template_rva` (or `null`), `tls_template_bytes`, `tls_zero_fill_bytes` and `tls_index_rva` (or `null`).
- `tls_callback_rvas`: list of **unexecuted** callback entry addresses relative to ImageBase.
- `tls_callbacks_executed: false`: explicit guarantee that this **metadata inspector never runs PE code**, resolves imports or calls TLS callbacks.

Synthetic tests provide an **8-byte TLS data template** at RVA 0x1300, a four-byte index at RVA 0x1310, 16 zero-filled bytes, and callbacks at RVA **0x1000** and **0x1010**. All data and code pointers are synthetic metadata; no Xbox file is present. Negative cases corrupt the image-base high bits, start/end order, zero-fill budget, index address, callback-table RVA, individual callback VA and terminating entry and require safe rejection. Existing direct PE imports, delayed imports, runtime-function entries and .pdata negatives remain covered.

## Why this matters

A manual PE loader that resolves `kernel32.dll` and copies code pages is still incomplete if it ignores initialization of thread-local data and the correct callback lifecycle; independent Windows-native source-built DLL tests and Game Core API review are necessary. The parser only inventories metadata and deliberately does **not** model per-thread TLS allocations, callback order, DLL thread attach/detach, SEH, C++ EH, console GDK or real game behavior. The separately tested [Windows dynamic SEH fixture](NATIVE_WIN_DYNAMIC_SEH_UNWIND_2026-11-22.md) proves the native host unwind API on a separate synthetic frame; it does not prove TLS.

Source: [Microsoft PE format, TLS Directory](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#the-tls-section) and [TLS implementation in PE](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format#the-tls-directory). No external GPL code, retail data, background schedulers or elevated Windows services are included.
