# Native Windows DXGI frame latency waitable object (one frame)

Extension to the original native Win32/D3D12 WARP [DXGI Present](NATIVE_WIN_DXGI_PRESENT_2026-11-22.md) test, built from the same original C++17 source. The normal Present test is unchanged; the second CTest invokes `native_win_dxgi_present --frame-latency-one`.

In the second test the original Win32 program requests `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT` on a 2-buffer `DXGI_SWAP_EFFECT_FLIP_DISCARD` chain, queries `IDXGISwapChain2`, calls **`SetMaximumFrameLatency(1)`**, retrieves `GetFrameLatencyWaitableObject()`, waits for an OS signal with a bounded 5-second deadline, closes the waitable handle, and continues to issue a D3D12 WARP buffer clear, real `Present(1,0)` and fence completion. The test does not create a background service or change Windows display system configuration.

CI must report an actual **Pass** rather than a capability **Skip** before claiming the waitable-object call path worked on GitHub's Windows runner. Unsupported DXGI mode returns CTest SKIP 77, rather than inventing an accepted setting.

Important: `SetMaximumFrameLatency(1)` constrains application frame queuing. It is **not** NVIDIA Reflex, DLSS, AMD Anti-Lag, or independently measured input-to-photon latency. The test uses a software WARP adapter and does not measure physical display response, p95/p99 latency, VSYNC stalls, driver scheduling or FPS. Those require real supported hardware, reliable OS/application input/simulation/Present time markers and controlled measurements before production configuration decisions.

Technical references: [Microsoft DXGI frame latency waitable object](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/waitable-object-swap-chains), [IDXGISwapChain2::GetFrameLatencyWaitableObject](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject), and [IDXGISwapChain2::SetMaximumFrameLatency](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-setmaximumframelatency).
