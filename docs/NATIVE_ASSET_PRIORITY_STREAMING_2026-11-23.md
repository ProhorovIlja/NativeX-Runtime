# Native bounded asset-stream priority lanes — 23 November 2026

## Purpose and limits

Original Windows/Linux C++17 worker-based asynchronous asset Reader now supports explicit priorities while keeping the old `Reader::submit(Request)` API, byte-exact reads, cancel token, maximum 4 MiB per request, bounded queue and safe shutdown. The new priority field is optional and defaults to `Normal`, so existing four-field aggregate initializers remain valid.

The change is useful foundational PC engineering for handling **frame-critical world/vehicle texture reads separately from normal streaming and low urgency prefetch** in a future native executable. However, this code **does not decode RPF9, BCPack, shader data, or the GTA VI streaming graph** and does not demonstrate any GTA VI performance improvement or DirectStorage implementation. No game content or EXE is present.

## Source-built implementation

- [Reader interface](../src/native_asset_stream.hpp), [Reader and scheduler](../src/native_asset_stream.cpp), [portable tests](../fixtures/native_asset_stream_tests.cpp).
- Three independent FIFO lanes inside the same worker pool: `FrameCritical`, `Normal`, `Background`. The existing `max_queued` limit applies to **all lanes combined** (not to each independently); workers and queue depth remain validated.
- Deterministic weighted round-robin selection when lanes are ready: **4 FrameCritical : 2 Normal : 1 Background** per 7 dispatches. This prevents permanent starvation of background asset reads when foreground reads continuously arrive. A lane that is empty is skipped, allowing other ready work to proceed.
- The wheel is a **scheduling policy, not a deadline or latency guarantee**. Real OS thread preemption, disk headroom, priority inversion inside a decoder, callback duration, file-system cache and large read tasks still determine latency. There is no worker-core pinning or automatic FPS detection.
- `select_next_queue(counts, cursor)` is a pure deterministic function for inspectable scheduling tests. The Reader uses the exact same chooser under its mutex; it consumes one accepted work item per dispatch, using a single shared cursor. The cursor normalization is bounded even on malformed test input.
- Existing behavior retained: invalid read ranges and unknown priority values reject, file offsets are overflow-checked, cancelled tokens are respected before and after reads, worker shutdown drains accepted work and fills all futures. `std::filesystem` paths are only opened by the worker through portable `std::ifstream`, not direct GPU or kernel APIs.
- No live or scheduled background monitoring, new persistent service, automatic download, game process injection, or data exfiltration.

## Acceptance and tests

The synthetic test suite now verifies exactly 40/20/10 dispatches across 70 all-ready selections, non-starvation of background from every starting cursor within the seven-slot wheel, Normal-only/Background-only dispatch, empty queues, rejection of unknown priorities, exact file bytes through actual native asynchronous reads at FrameCritical and Background priorities, and all original read/cancel/shutdown/range checks.

Only source-built sample files are used. The test does **not** claim that actual game streaming, SSD DirectStorage, Linux io_uring, GPU residency or p99 frame-time has been measured.

## Why upstream developments matter

Public `R0Y00/Xbox2Windows` [PR #850](https://github.com/R0Y00/Xbox2Windows/pull/850), merged 23 November 2026, checks when a decoded archive record is safe to publish as a 4×4 RGBA8 texture, and ensures the incomplete second record never publishes. The independent AOT/PE native comparison **is not the same code as this priority scheduler**. Both behaviors matter: priority should affect *when to read* and decoding must decide *when a complete resource is allowed into GPU memory*. Neither establishes title boot.

[PR #503](https://github.com/R0Y00/Xbox2Windows/pull/503) from the same source also reports coverage for all 60 VEX FMA3 instruction forms with native-vs-lifted differential tests and strict NaN/rounding behavior. That is a CPU execution correctness result, not a measured GTA VI speedup. Our existing `native_cpu_dispatch` makes host ISA policy decisions and is **not** equivalent to an instruction-recompilation engine. MIT upstream code was reviewed, not imported in this change.

## Future real-game acceptance

After a lawful, verified game build is accessible, profile asset demand on the Ryzen 5 5600 / RTX 4070 12 GB / 32 GB RAM at 1080p. Measure per-request time-to-first-byte, worker queue waiting time, cancellation after scene changes, RAM/VRAM peak, SSD throughput and p95/p99 frame stalls before choosing queue weights. Consider batching requests, multi-resource deduplication, bounded decompression, format validation, texture residency and actual DirectStorage/io_uring adapters only once their behavior can be compared to real title data.

**Status:** implemented native host queue policy and synthetic Windows/Linux tests; not a playable GTA VI port, game-format decoder or frame-time optimization claim.
