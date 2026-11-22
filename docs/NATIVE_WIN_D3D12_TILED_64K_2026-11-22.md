# Native Windows D3D12 64 KiB reserved/tiled buffer memory test

Date: 2026-11-22. The original `fixtures/d3d12_warp_tiled64k.cpp` uses desktop Windows x64 D3D12 WARP to test the smallest relevant **real GPU virtual-memory tile** primitive, not a fabricated CPU mapping.

Test procedure: obtain a WARP D3D12 device, verify `D3D12_FEATURE_D3D12_OPTIONS` tiled resource tier, create a 128 KiB default heap of two 64 KiB pages, create a **reserved buffer** with `ID3D12Device::CreateReservedResource`, map its two virtual tiles onto the heap using the real `ID3D12CommandQueue::UpdateTileMappings`, copy independent deterministic byte patterns into both tiles, transition from COPY_DEST to COPY_SOURCE, copy into a READBACK heap, wait on a D3D12 queue/fence and compare **every one of 131,072 bytes** against the CPU oracle.

- If supported, this demonstrates real Windows tiled-resources mapping, GPU copy and exact CPU readback on the selected **software WARP** adapter. It is not a Radeon/NVIDIA/Intel hardware GPU result or a bandwidth/latency benchmark.
- If tiled resources or reserved buffers are not available on the CI host, the test returns CTest **SKIP 77** and does **not** claim success. Unexpected device loss or data mismatches fail CI.
- This does **not** implement the Xbox Series UMA 64 KiB physical page API, guest virtual addresses, CPU/GPU page coherency, read/write-watch, shared-alias barriers, GPU page faulting, console D3D12.X packets, the Rockstar allocator or actual game rendering.

## Source research and integration boundary

The 2026-11-22 [XCL commit `660f6dc2f0ee`](https://github.com/ivall/xcl/commit/660f6dc2f0eec8a14cf69c7cae0d8f1813da1fa1) describes a separate Rust GPU sparse-window strategy: up to three 4 GiB UMA reserved buffers, tile mapping for committed 64 KiB title pages, 64-byte changed-region synchronization before GPU submits and console root-argument/vertex-buffer translation. Its author reports XDefiant running 90 seconds with **black frames** and no GPU hang. This is **upstream evidence, not a locally reproduced result**, and does not prove GTA VI compatibility. XCL is GPL-2.0 per current repository metadata; no implementation was copied into this independent source test.

Relevant public Windows APIs: [D3D12 tiled resources](https://learn.microsoft.com/en-us/windows/win32/direct3d12/volume-tiled-resources), [UpdateTileMappings](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-updatetilemappings), [CreateReservedResource](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12device-createreservedresource).

## Next acceptance

Real hardware testing with AMD RDNA2-class and modern NVIDIA/Intel GPUs, aliasing and cross-queue hazards, multiple mappings to one tile, CPU+GPU coherency, residency budgets and page-fault handling. Then compare against the real console build and asset memory traces when legally available. Do not infer performance or title compatibility from a synthetic WARP buffer.
