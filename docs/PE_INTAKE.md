# PE32+ read-only intake

The independent native C++17 tool `pe_intake` parses a bounded (512 MiB maximum) local PE32+ AMD64 file, **without executing any of its instructions**.

The JSON report contains:
- PE machine type, entry RVA, image base and section names;
- regular import DLL names;
- RVA-based delay-import DLL names;
- presence of a minimum 40-byte PE32+ TLS directory;
- number of base relocation directory blocks.

All addresses are bounds-checked in file-offset/RVA space. Unsupported absolute-VA delay import descriptors are rejected rather than interpreted as safe. This is a **metadata inspector** only: it does not resolve TLS callbacks, execute initializers, decode console graphics or imply title compatibility.

CI uses a generated synthetic PE32+ input with deliberately invalid cases for relocations, TLS-directory size and delay-import descriptors. No proprietary executable or game file is used.

Future work: named/ordinal import table entries, exported symbols, relocation types, exception/unwind metadata, delay import attribute variants and property-based fuzzing.
