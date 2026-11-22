# Native Windows/Linux frame phase timing and diagnostic percentiles

Original portable C++17 `src/native_frame_timeline.{hpp,cpp}` and `fixtures/native_frame_timeline_tests.cpp`. This is an **on-demand local profiling primitive** with no background task, network transfer, daemon or OS tracing hooks; it cannot observe a GTA VI process.

The original `Timeline` maintains a fixed 256-frame buffer in the calling thread and performs **no heap allocations while recording**. A `Phases` record has nanosecond-unit monotonic host stamps for an optional input receipt, simulation begin/end, graphics submission and the CPU return from `Present`. Samples are rejected before mutation if ordering is invalid, input is later than simulation start, or a measured span exceeds 60 seconds. The oldest valid sample is overwritten when the bounded ring fills, with an explicit overwrite count.

The `summarize()` call is intended for a diagnostic/non-real-time thread **after a frame capture**, not in a time-critical render callback: it copies and sorts fixed arrays and computes exact **nearest-rank p50/p95/p99**, plus minimum/maximum durations for simulation, submission-to-Present-return, CPU simulation-to-Present-return and available input-to-Present-return spans. Missing input timestamps are omitted from the latter rather than assumed zero latency. The implementation assumes one owner thread; cross-thread timestamp forwarding requires explicit external synchronization.

Windows `monotonic_ns()` uses host `QueryPerformanceCounter/QueryPerformanceFrequency` with division before multiplying to avoid host-uptime overflow; native Linux uses `clock_gettime(CLOCK_MONOTONIC)`. Values are presented in nanosecond *units* but do **not** imply 1 ns physical clock resolution.

**Crucial limitations:** Present's API return is not a monitor scanout or first visible photon, and submitted command time is not GPU completion. These per-frame spans are CPU-side software timing diagnostics, **not** measured game input lag or frame time on a physical display. NVIDIA Reflex, Intel XeLL, AMD latency features, VRR/HDR, WARP, GPU driver pipeline depth, audio-video synchronization, motion-to-photon performance and real GTA VI game loops require independent title/hardware integration. One should avoid comparing arbitrary CI runner latency to target Windows 10/11 gaming desktops.

Five deterministic Linux/Windows tests cover empty state, invalid timestamp rejection, nearest-rank quantiles (100 samples), 256-frame eviction, missing input time and native monotonic clock. No physical GPU, game EXE, external dependencies or automatic monitoring.

Related verified platform controls: [Win32 DXGI one-frame swapchain waitable object](NATIVE_WIN_DXGI_LATENCY_2026-11-22.md) is a separate concrete API acceptance. Combining both may later enable measured frame-time and CPU-side queue time analyses, but does not independently prove input-to-photon latency.
