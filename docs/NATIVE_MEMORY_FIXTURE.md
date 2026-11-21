# Native Windows memory reservation and alias differential fixture

This new source-built Windows x64 fixture verifies real Win32 virtual-memory behaviors needed as a **baseline** for any Xbox Series native compatibility runtime:

- `VirtualAlloc(MEM_RESERVE)` leaves pages uncommitted; `VirtualQuery` checks reservation vs commitment.
- Two interior pages are committed independently; `VirtualProtect` is verified and restored.
- A decommitted/recommitted private page reads zero without changing its adjacent committed page.
- A pagefile-backed `CreateFileMappingW` object is mapped at **two distinct host virtual addresses** via `MapViewOfFile`.
- Two-way byte modifications of each view are checked through the other; the entire mapping is scanned deterministically.

The test never intentionally reads or writes an uncommitted/protected page and never installs an exception handler to hide access violations.

**Limits:** These are native Win32 semantics, **not** Xbox `XMemMapPhysicalPages` or Xbox unified-memory semantics. No GPU aliasing, device virtual address, shader, cache flush, or game input was exercised. An implementation which pretends Win32 file-mapping aliases are identical to console physical pages is still incorrect.

Relevant source research: [XCL physical-page mapping commit](https://github.com/ivall/xcl/commit/fd746622e95dcb3a4b6b8a985ea045a1a3c6610a). The XCL implementation is an upstream author-reported compatibility strategy, not reproduced here.

Source-built only, no commercial game or SDK. CI runs this test exclusively on standard public Windows x64 runners.
