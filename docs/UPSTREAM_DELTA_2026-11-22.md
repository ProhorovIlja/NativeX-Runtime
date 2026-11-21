# Upstream delta review — 2026-11-22

Point-in-time, read-only public GitHub review. These are exact upstream commit/PR observations and author-reported results, **not** independent execution of an Xbox game or proof that GTA VI can boot. This project has no matching GTA VI console executable or assets.

## New blocking evidence

**[ivall/xcl e555b87](https://github.com/ivall/xcl/commit/e555b87c07482c139c18baf0c20effa51b4b3938)** (author report, Windows native x64 GDK compatibility) documents attempted execution of an Xbox console title. The reported fixes affect D3D12.X compatibility:

- The game provides console-sized `D3D12_OPTIONS` structures that differ from PC sizes; bad feature queries should not produce unconditional success.
- `ID3D12Device` method #25 `GetResourceAllocationInfo` involved a return-aggregate / hidden-result calling-convention hazard. A wrong shim wrote into a device vtable.
- Console DMA lists have a distinct method map; a desktop D3D12 command-list vtable is not a general substitute.
- **Key unsolved issue:** inlined code writes console graphics command records directly into command-list object fields. Desktop D3D12 objects have different internal layouts, so forwarding the method names is insufficient.

These are observations from the author's title and toolchain; memory layouts and slot indexes must be independently validated for any other title/build. Our corresponding tracking issue is [#3](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/issues/3). No XCL GPL-2.0-only source is copied.

## RDNA2 shader translation

**[R0Y00/Xbox2Windows #279](https://github.com/R0Y00/Xbox2Windows/pull/279)** was merged: `pixel_aot` gained more 64-bit scalar ALU operations and 32-bit bitfield extracts, with synthetic test cases. This is evidence of translation *coverage*, not GPU correctness for GTA VI shaders.

**[ItsNotRexed/Leonida #209](https://github.com/ItsNotRexed/Leonida/pull/209)** was merged: mesh shader `v5` is now seeded as a group index times `NumThreads` plus local lane, addressing a meshlet-indexing mismatch. [Commit 3fae38a](https://github.com/ItsNotRexed/Leonida/commit/3fae38a7a16ef1e9803349495598ff79510a8296) adjusted associated test expectations. This is not verified on Xbox hardware.

**[Leonida #204](https://github.com/ItsNotRexed/Leonida/pull/204)** remained **open** according to its GitHub PR API when checked, despite a merge-like commit subject observed in the main branch history. Its author reports a native AMD wave64 `ds_bpermute` problem for missing lanes of a partial workgroup; the fix uses a ballot to avoid reading non-running invocations. This behavior is **not** interchangeable with the undefined inactive-lane read semantics of HLSL `WaveReadLaneAt`.

**[AnyXboxX fac3673d](https://github.com/LooperSalty/AnyXboxX/commit/fac3673dcfbc8a53160db52fbe5d1109738c894a)** added RDNA2 SMEM operand disassembly support; author claims native execution of two public-source GDK samples elsewhere in the repository. A direct independent source build and run remains unverified in our project ([#14](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/issues/14)).

## Independently verified host capabilities

Our own Windows CI has already exercised and compared:
- source-built PE32+ import/export/forwarder metadata and our compiled DLL exports;
- Windows virtual memory commit/protect/decommit and shared mapping aliasing;
- source-built SM5 WARP D3D12 compute and readback;
- source-built HLSL SM6 WaveOps on WARP, compared to CPU reference.

The first hardware-adapter test accidentally accepted a **Microsoft DXGI adapter** with vendor ID `0x1414` as hardware, despite a missing software flag. We corrected the gate to accept only AMD `0x1002`, NVIDIA `0x10de` or Intel `0x8086`, still rejecting adapters with `DXGI_ADAPTER_FLAG_SOFTWARE`. On the hosted Windows runner the physical GPU case was then correctly **SKIPPED** ([run 44954553642](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44954553642)). The WARP case passed. A virtualized adapter could still masquerade as a listed vendor, so driver/hardware acceptance remains to be performed.

## Native port engineering order

1. Preserve actual Windows x64 executable semantics and source-level ABI correctness; synthetic tests must include hidden struct-return calling conventions and COM method maps.
2. Progress from virtual-memory aliases to a source-backed page identity/remapping scheme with explicit dirty tracking and CPU/GPU coherency tests.
3. Build independently tested RDNA2→desktop shader lowering and command-record→D3D12 translation with known synthetic data; avoid classifying a successful compile as a rendered frame.
4. Reproduce optional public-source native GDK examples on Windows without CPU emulation and record exact source/SDK dependency versions. Do not import third-party GPL/LGPL sources into this MIT repository without a full license review.
5. Only when a lawfully obtained matched title build is available, perform local-only read-only inventory, missing-service comparison and correctness/boot tests. No retail files or proprietary blobs go into GitHub or Actions.

**Final product criterion:** a native Windows x64 executable/app with Windows graphics/audio/input/saves, not a console VM or Unicorn runtime. See [native target definition](NATIVE_TARGET.md).
