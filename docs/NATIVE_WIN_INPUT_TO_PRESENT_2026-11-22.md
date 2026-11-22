# Native Win32 integration: frame input snapshot → D3D12 Present → phase timing

Original source-built Windows x64 acceptance `fixtures/native_win_input_to_present.cpp` (2026-11-22), independent from the actual GTA VI title.

This **integrates three previously independent tested C++17 components** rather than adding another disconnected API initialization:

1. The portable `native_input_core` receives *original synthetic* per-device keyboard and mouse transitions. A frame captures the immutable held-key/button snapshot while the underlying input state is subsequently released; later queries confirm the frame's copy remains correct. No actual keyboard/mouse hardware events, controllers, hooks or virtual devices are injected by this synthetic test.
2. The same Windows process creates the actual Win32 HWND, D3D12 **software WARP** graphics queue and 64×64 double-buffered DXGI flip chain, submits a backbuffer-clear command and calls `IDXGISwapChain3::Present`, then fences completion. The backend is the real Windows D3D12 API, though hardware acceleration and console shaders remain unverified.
3. Monotonic Windows `QueryPerformanceCounter` stamps bracket the synthetic input receipt, simulation begin/end, graphics submission and **CPU return from Present**. The `native_frame_timeline` module records those stages and verifies the bounded p50/p95/p99 collector accepted one valid sample. The JSON reports CPU-side simulation-to-Present-return and input-receipt-to-Present-return sample durations in nanosecond units.

If a Win32 interactive presentation surface or WARP DXGI is unavailable, CTest explicitly **SKIPs (77)**. Invalid state, a failed `Present` when the device is lost, a fence timeout or an invalid timing sequence fail the test. No real game EXE/assets/OS background service is used.

## What it **does not** prove

This is one frame with synthetic controls and a solid-color render target; it does **not** demonstrate real Raw Input hardware event-to-pixel latency, controller polling, gameplay simulation, frame pacing across hundreds of frames, physical GPU performance, input-to-photon timings, Rockstar's Xbox GDK/D3D12.X API or any GTA VI gameplay. It is a small verifiable **desktop platform wiring test**, not a unified game runtime or port.

For real playability, replace the synthetic frame input with actual foreground device events, add stable frame scheduling, assign game state updates, use shader/material/asset information from an authorized game build and evaluate per-GPU hardware, page memory semantics and latency distributions. The Windows native single-player port remains the prerequisite; Linux native ELF and optional Proton path are distinct targets.

Related exact source validations: [Raw Input semantics](NATIVE_PC_INPUT_2026-11-22.md), [frame timing](NATIVE_PC_FRAME_TIMING_2026-11-22.md), [windowed D3D12 Present](NATIVE_WIN_DXGI_PRESENT_2026-11-22.md), and [one-frame DXGI queue](NATIVE_WIN_DXGI_LATENCY_2026-11-22.md).
