# Native Present-return frame pacing diagnostics — 2026-11-23

## Why this exists

The user correctly challenges unsupported public claims that GTA VI on PC will require a specific CPU (e.g. Ryzen 5 5600) for 30 FPS, or that 60 FPS is automatically feasible on older processors. No measured GTA VI PC frame times or officially published PC minimum hardware requirements are available to this project. Instead of baking either speculation into a hardware gate, extend the existing **source-built local diagnostics** so later hardware acceptance can measure meaningful timing.

`src/native_frame_timeline.hpp/.cpp` now includes `Summary.present_cadence`: a bounded, allocation-free analysis of intervals between **consecutive calls returning from Present**. The existing phase measurements remain unchanged (simulation duration, render submit→Present return, simulated CPU frame time, and software input→Present-return proxy). This extension adds:
- `PresentCadence.interval`: minimum/median/p95/p99/maximum of positive, plausible intervals between successive Present return timestamps in a 256-sample owner-thread ring. A ring with N samples has at most N−1 intervals; an empty/one-sample ring has **zero**, not a fabricated frame rate.
- `missed_30hz`, `missed_60hz`, `missed_120hz`: counts of valid intervals exceeding the rounded-up nanosecond deadline `ceil(1e9/Hz)` (33,333,334 ns, 16,666,667 ns, 8,333,334 ns). Exact rounded-up deadlines are **not** wrongly reported as misses.
- `invalid_intervals`: duplicates, reversed timestamps, and discontinuities longer than 60 seconds, excluded from all percentiles/miss counts. A later valid interval is still included.
- All calculations are deterministic integer arithmetic, fixed-memory and without allocated helper buffers; there are no extra threads, auto-starting profilers, telemetry uploads or scheduled tests.

## Interpret with care

**`Present` returning is not the monitor scanning out a frame.** This measures host software call-return spacing; an unsynchronized or queue-ahead renderer can call Present rapidly while the GPU/display produces fewer frames. It does not measure GPU bottlenecks, actual displayed FPS, frame interpolation quality or real input-to-photon latency.

For real performance claims, instrument actual D3D12/DXGI presentation statistics, GPU timestamp queries (with calibrated clocks and queue completion), simulation and RAGE render workload, asset streaming stalls, and memory/VRAM pressure on each physical target GPU. Compare 1%-low/p95/p99 frame intervals under consistent resolution, area, traffic/weather, draw distance and fixed graphics feature settings. Report **native rendered FPS separately from generated/displayed FPS** and do not claim that upscaling, FG, a new programming language or DirectStorage removes a CPU simulation bottleneck.

At 30/60/120 FPS, a full-frame budget is about **33.33/16.67/8.33 ms**, respectively; these are **target intervals**, not predicted GTA VI results. When no lawful copy/executable can run, source-built synthetic tests cannot establish CPU minimums.

## Tests

The Windows/Linux deterministic suite `native_frame_timeline_tests` gains 3 scenarios (5→8):
1. Six known exact Present-return gaps (16,666,667 ns, 33,333,334 ns, 8,333,333 ns, 8,333,334 ns, 1 s) and independently checked quantiles and per-target deadline misses, while each simulated CPU phase remains only 2 ms.
2. Consecutive duplicate, reversed and >60-second discontinuous timestamps must produce no false positive frame throughput; the subsequent valid interval is accepted.
3. A 260-frame ring wrap retains exactly the most recent 256 frames and 255 adjacent valid intervals without crossing the overwritten boundary, with deterministic 30/60/120 deadline counters.

Reference: [Microsoft DXGI frame statistics and Present semantics](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/dxgi-present), [Microsoft PresentMon](https://github.com/GameTechDev/PresentMon) for future hardware profiling. This code does not integrate third-party software or use game files.

No GTA VI executable, graphics hardware measurement, CPU-frequency calibration, cross-frame GPU timestamp, continuous telemetry, background service, new scheduled task or external notification is included.
