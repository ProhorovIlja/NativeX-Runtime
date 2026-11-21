# Native desktop Direct3D 12 / WARP smoke test

This original Win32 C++17 executable uses **only the Windows SDK** and desktop D3D12 + DXGI:

1. Create a DXGI factory; request the software WARP adapter.
2. Create a real D3D12 device, command queue, allocator and command list.
3. Close and submit an empty command list, signal a fence and wait (10 s bound).
4. Ask the device about its wave operation support and lane ranges via \`D3D12_FEATURE_D3D12_OPTIONS1\`.
5. Fail if a created device cannot execute the command list, signal its fence or reports removal.

No GPU hardware, proprietary shader, game EXE, console library or driver changes are required. On hosts without WARP/D3D12, the fixture exits **77 (CTest skip)** rather than incorrectly reporting compatibility.

This test confirms **desktop D3D12 queue/fence semantics on a Windows host** when it passes. It does *not* check actual wave intrinsics, RDNA2 machine code, D3D12.X command signatures, Xbox GPU memory layout, game performance or real-world rendering. A WARP adapter is a CPU software renderer, not evidence that an Xbox GPU is supported.

Relevant comparison: [Microsoft D3D12 feature options](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_d3d12_options1) and [Microsoft WaveGetLaneCount](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/wavegetlanecount).

CI uses the existing public Windows standard hosted runner. No paid or GPU-enabled runner type.
