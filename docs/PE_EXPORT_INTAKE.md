# PE32+ export directory and forwarder intake

`pe_intake` now reports exported names, ordinal numbers and forwarded-export target strings from `IMAGE_EXPORT_DIRECTORY` **without resolving or calling the exports**. Read-only parsing validates:

- PE32+ export directory length and per-section file ranges;
- bounded function/name/ordinal tables (limits of 8192 functions and 4096 names);
- ordinal index within the function address table;
- names and forwarded target strings fully mapped in the file's PE RVA space;
- directory-range classification of forwarders and malformed table rejection.

The generated synthetic test includes 3 named exports, base ordinal 5, and a forwarded export. Corrupted function counts, table RVAs and ordinal indices must fail. On Windows, an additional integration test scans **our own compiled fixture DLL** and expects exported functions `ScarlettAdd` at ordinal 1 and `ScarlettMultiply` at ordinal 7.

This metadata inspector is intentionally **not** a PE loader: it does not map instructions for execution, patch an IAT, resolve imports, load console libraries or bypass any title restrictions. It cannot determine whether an Xbox title runs from its exports alone.

No commercial games, privileged interfaces or proprietary SDK files are required.
