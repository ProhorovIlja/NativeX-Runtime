# Native Windows SM6 wave differential test (optional)

A source-built HLSL `cs_6_0` shader records `WaveGetLaneIndex`, `WaveGetLaneCount`, `WaveReadLaneAt(lane0)` and `WaveActiveSum`. On a supported WARP device, native Windows D3D12 runs the shader, copies output to CPU and checks lane uniqueness, lane-to-leader IDs and subgroup sums without assuming contiguous global IDs.

A trusted Windows SDK `dxc.exe` is required. CI searches installed Windows Kits without downloads. If absent, CMake explicitly omits the test; if WARP lacks WaveOps or Shader Model 6, CTest marks it **SKIPPED (77)**. Failing shader creation or output comparison is a test failure.

A passing test proves only **desktop Windows D3D12 SM6 WaveOps on WARP**, not Xbox RDNA2 code, native hardware wave64, D3D12.X or a running retail title. NVIDIA/AMD/Intel hardware differential checks remain future work. Source references: [Leonida #94](https://github.com/ItsNotRexed/Leonida/issues/94), [Leonida PR #204](https://github.com/ItsNotRexed/Leonida/pull/204), [D3D12_OPTIONS1](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options1).


## Physical adapter mode

Run `d3d12_wave_sm6 <path.dxil> hardware` to enumerate DXGI adapters while excluding software devices. Only devices supporting D3D12 Shader Model 6.0 and WaveOps enter the same source-built dispatch and CPU readback verification as WARP. Output includes vendor/device IDs, group count and backend. On machines without a compatible hardware adapter the test returns 77, recorded as **SKIPPED** by CTest — never as a hardware pass.

Windows CI runs WARP and hardware tests separately. Standard public runners may not expose a suitable hardware GPU; that is a limitation, not a failure of the native port or proof of hardware support.


### CI finding: software adapter with missing software flag

A Windows GitHub runner reported a D3D12 WaveOps pass on DXGI vendor ID **5140 (0x1414, Microsoft)** and device ID **140** while the adapter was *not* marked `DXGI_ADAPTER_FLAG_SOFTWARE`. That is **not an AMD/NVIDIA/Intel hardware validation**. The hardware test was therefore strengthened to require an AMD (0x1002), NVIDIA (0x10DE), or Intel (0x8086) vendor ID *and* no software flag. The software WARP test remains independent. On ordinary hosted Windows runners the vendor-gated hardware case may now correctly be SKIPPED (77).

This is still insufficient as proof of physical device identity in unusual virtualized environments; an actual GPU driver and hardware machine report are needed for cross-vendor qualification.
