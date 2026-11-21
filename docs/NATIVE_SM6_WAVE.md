# Native Windows SM6 wave differential test (optional)

A source-built HLSL `cs_6_0` shader records `WaveGetLaneIndex`, `WaveGetLaneCount`, `WaveReadLaneAt(lane0)` and `WaveActiveSum`. On a supported WARP device, native Windows D3D12 runs the shader, copies output to CPU and checks lane uniqueness, lane-to-leader IDs and subgroup sums without assuming contiguous global IDs.

A trusted Windows SDK `dxc.exe` is required. CI searches installed Windows Kits without downloads. If absent, CMake explicitly omits the test; if WARP lacks WaveOps or Shader Model 6, CTest marks it **SKIPPED (77)**. Failing shader creation or output comparison is a test failure.

A passing test proves only **desktop Windows D3D12 SM6 WaveOps on WARP**, not Xbox RDNA2 code, native hardware wave64, D3D12.X or a running retail title. NVIDIA/AMD/Intel hardware differential checks remain future work. Source references: [Leonida #94](https://github.com/ItsNotRexed/Leonida/issues/94), [Leonida PR #204](https://github.com/ItsNotRexed/Leonida/pull/204), [D3D12_OPTIONS1](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options1).
