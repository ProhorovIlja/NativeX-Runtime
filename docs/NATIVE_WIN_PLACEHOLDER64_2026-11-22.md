# Native Windows placeholder-address remapping of actual 64 KiB CPU pages

Original Windows x64 source-built `fixtures/native_win_placeholder64.cpp`, created 2026-11-22. Unlike the earlier two `MapViewOfFile` aliases, this test exercises the **actual Windows 10+ placeholder API family**, enabling host code to reserve a contiguous virtual address range and replace selected pages at exact virtual addresses without releasing the address reservation.

## Exact OS actions

The test loads the documented user-mode Windows APIs `VirtualAlloc2`, `MapViewOfFile3` and `UnmapViewOfFile2` dynamically from kernelbase/kernel32. It checks the host's native allocation granularity is **65,536 bytes**. No external driver, elevated privilege, emulator, heap-copied surrogate or proprietary Xbox binary is required.

1. Reserve **four contiguous 64 KiB slots (256 KiB)** using `MEM_RESERVE|MEM_RESERVE_PLACEHOLDER`, then split the contiguous reservation into **four individual 64 KiB placeholders** using `VirtualFree(MEM_RELEASE|MEM_PRESERVE_PLACEHOLDER)`.
2. Create a separate pagefile-backed **two × 64 KiB** section with `CreateFileMappingW`. In non-monotonic order, replace slot 3 with backing offset 0 and slot 1 with backing offset 64 KiB using `MapViewOfFile3(MEM_REPLACE_PLACEHOLDER)`. Fill both backing pages and verify **all 65,536 bytes** in each mapped view.
3. Map slot 0 to **the same physical section offset as slot 3** and verify that a write through one virtual view appears through the other. Attempt illegal replacement of an already-mapped view and a wrong-size view; these operations must fail without mutating existing data or the remaining placeholder.
4. Use `UnmapViewOfFile2(MEM_PRESERVE_PLACEHOLDER)` on slot 3. Confirm the virtual address is reserved without dereferencing the unmapped hole and the surviving alias in slot 0 preserves the backing contents. Map slot 2 to backing offset 64 KiB, unmap slot 1, mutate through slot 2, and map slot 3 again to backing offset 0. Verify values and placeholder states. Cleanup explicitly unmaps mapped slots back to placeholders and releases all four sections.

A missing API / unsupported OS returns a **CTest SKIP 77** rather than a false pass. Once the APIs and initial reservation are operational, failed address replacement, incorrect contents, overlapping mappings, wrong-state placeholders or unexpected errors fail CI.

Microsoft primary contracts: [VirtualAlloc2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2), [MapViewOfFile3](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-mapviewoffile3), [UnmapViewOfFile2](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-unmapviewoffile2) and [VirtualFree placeholder splitting](https://learn.microsoft.com/en-us/windows/win32/api/memoryapi/nf-memoryapi-virtualfree).

## Limitations

These are *real host-CPU* virtual memory mappings, not a separately verified Xbox XMem allocator, guest page tables, GPU tile residency, real coherent CPU-GPU UMA or Rockstar title code. The original [D3D12 WARP GPU tile alias](NATIVE_WIN_D3D12_TILE_ALIAS_2026-11-22.md) tests GPU memory in a separate API/address space; passing both does **not** prove they are linked. Additional page identity/lifetime, synchronization, GPU cache visibility, memory protection races, backing-store growth and actual AMD/NVIDIA/Intel driver acceptance remain mandatory.

This fixture is useful for building an original Windows-native host virtual-memory adapter with exact address identities, but no claim about game playability or DirectStorage throughput should be inferred.
