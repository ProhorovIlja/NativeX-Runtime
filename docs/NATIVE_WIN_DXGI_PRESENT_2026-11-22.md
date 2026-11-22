# Native Windows x64 D3D12 WARP DXGI HWND Present attempt

Updated 2026-11-22. A source-built original Windows C++17 fixture `fixtures/native_win_dxgi_present.cpp` attempts a **real foreground Win32 HWND**, desktop DXGI factory, WARP software adapter, D3D12 device and direct command queue, double-buffered `DXGI_SWAP_EFFECT_FLIP_DISCARD` swapchain, render-target views, backbuffer transition PRESENT→RENDER_TARGET→PRESENT, clear command, `IDXGISwapChain3::Present(1,0)`, queue fence completion and device-removal check.

This complements the earlier actual offscreen [D3D12 WARP raster/readback](D3D12_RASTER_READBACK_2026-11-22.md) test, which already verified synthetic triangle pixels but did **not** use a window or call Present. The two tests exercise different contracts.

## CI interpretation

- If the host can create an HWND presentation surface and DXGI accepts the flip chain and Present call, the JSON reports `present_called_and_succeeded=true`; after a successful fence wait this confirms native DXGI API submission and WARP flip model. It does **not** confirm that a human actually saw a pixel, no tearing, HDR, hardware acceleration, frame pacing or actual game rendering.
- Standard Windows GitHub Actions workers may run with no interactive desktop or presentation output. If Win32 HWND/DXGI WARP or the session's Present capability is unavailable, the test emits an explicit `SKIP` with HRESULT and returns **77**. The CTest mark is *skipped*, not successful evidence of Present. Unexpected device-lost errors and command/fence failures are hard failures.
- It creates no full-screen state, no vendor SDK or retail EXE. It uses Microsoft Windows SDK and the system WARP software rasterizer only, no new packages or scheduled workflows.
- Headless CI **may not** be sufficient; acceptance requires a local interactive Windows 10/11 desktop with a physical AMD/NVIDIA/Intel device, DXGI debug layer and actual frame presentation/pacing measurements. Hardware presentation is a separate test from WARP.

## Later integration requirements

- Native Direct3D12 renderer window/swapchain ownership, resize/device-lost handling, active display HDR/VRR/tearing support, multi-monitor, presentation feedback, frame-time distributions and integration with input timestamps.
- Actual GPU shader generation from validated title material, accurate Xbox D3D12.X resource state translation and render passes. An ordinary desktop D3D12 frame is not proof that Xbox GPU commands or shaders render properly.
- Optional FSR/DLSS/XeSS and latency APIs require verified official SDKs, GPU support, motion/depth guides and real frame graphs; third-party DLSS 5 injectors do not automatically become compatible by loading a DLL.

See [Microsoft DXGI CreateSwapChainForHwnd](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgifactory2-createswapchainforhwnd) and [DXGI Present](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present).
