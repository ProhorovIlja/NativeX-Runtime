# Native Windows D3D12 two virtual tiles aliasing one 64 KiB backing tile

Research fixture from 2026-11-22. Original C++17 program: `fixtures/d3d12_warp_tile_alias.cpp`. It extends the independently verified [D3D12 64 KiB tiled-memory baseline](NATIVE_WIN_D3D12_TILED_64K_2026-11-22.md) without importing XCL or other licensed runtime implementation.

It requests a real software **D3D12 WARP** device with tiled-resources support, creates a **64 KiB** D3D12 heap and a **128 KiB** reserved GPU buffer, and invokes the actual `ID3D12CommandQueue::UpdateTileMappings` with `D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE`. Both virtual 64 KiB regions are mapped to **the same** 64 KiB backing tile.

Next, a real D3D12 command buffer writes the original deterministic 64 KiB pattern to the **first virtual tile only**. After a COPY_DEST→COPY_SOURCE state transition, the command queue copies all **128 KiB** into a separate CPU readback resource. A queue fence is required before comparing every single byte. Both virtual regions must return the same 64 KiB source pattern. Any mismatch or device removal fails CI. An unsupported tiled-resources device returns **CTest SKIP 77**; a skip is never represented as a successful alias test.

What this test can prove: native Windows software D3D12 memory mapping semantics between two overlapping virtual GPU tile addresses and one backing heap tile. This is stronger than separate 1:1 mapped tiles but **does not** establish a console guest-physical page allocator, Xbox Series UMA CPU-GPU coherency, guest virtual memory mapping, coherency through real shader writes, deterministic GPU residency or actual Rockstar game resources.

Recent research context: GPL-licensed [XCL `660f6dc2f0ee`](https://github.com/ivall/xcl/commit/660f6dc2f0eec8a14cf69c7cae0d8f1813da1fa1) implements upstream-author-described 64 KiB console page reservations and larger GPU sparse virtual windows; its reported title run still shows black frames. None of that implementation is copied into this MIT project. The exact D3D12 API contract is documented by Microsoft in [UpdateTileMappings](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-updatetilemappings), including `REUSE_SINGLE_TILE` semantics.

A full port later needs separate tests of non-aliased residency, mixed read-write device barriers, memory-bus visibility, synchronization on true AMD/NVIDIA/Intel physical GPUs, and comparison with legitimately available Xbox game file/EXE behavior.
