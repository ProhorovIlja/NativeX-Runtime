# CPU page-identity and dirty-upload reference model

This **original C++17 model**, running on Linux and native Windows, supplies a deterministic correctness contract for a possible console-style physical-page pool.

It is not an Xbox implementation. Physical page objects are represented by 64 KiB synthetic byte arrays; the virtual-address region is represented by 8 logical slots rather than OS page-table operations.

The tests cover:

- reverse-order mapping of three independent page IDs into eight slots;
- byte preservation at both ends of a page;
- prevention of accidental double mapping and collisions;
- map → move → unmap → remap while the backing page retains its data;
- invalid page and slot indexes fail without changing state;
- unmapped texture-preview slots explicitly produce zeros (a **model choice**, not legal CPU access);
- dirty-slot tracking: staged/previous snapshots stay stale until an explicit upload; moves invalidate the former and new slots, unmap clears them.

This model will be useful for independently checking a future native Win32 address-space mapping runtime without adopting upstream source code. It does **not** emulate CPU instructions, call `XMemMapPhysicalPages`, manage real physical pages, or issue GPU commands.

Comparison references: [XCL physical reservation/remap experiments](https://github.com/ivall/xcl/commit/fd746622e95dcb3a4b6b8a985ea045a1a3c6610a) and [our native Win32 allocation/alias fixture](NATIVE_MEMORY_FIXTURE.md). The next gate is a real Windows implementation with source-built pages, source-controlled dirty tracking and D3D12 upload/readback, explicitly noting deviations from Xbox hardware semantics.
