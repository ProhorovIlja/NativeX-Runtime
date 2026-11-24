# Portable native Windows and Linux asset streaming baseline

This is a real, source-built asynchronous **worker-thread + filesystem** implementation in C++17, not merely a planned API. It reads deterministic synthetic local files on Windows and Linux with asynchronous futures, fixed-size request limits, cancellation, offset/EOF validation, FNV-1a integrity checks and clean queue draining on shutdown.

Files:
- src/native_asset_stream.hpp and src/native_asset_stream.cpp define bounded requests, status/error results and a native worker pool.
- fixtures/native_asset_stream_tests.cpp creates 2 MiB of its **own synthetic bytes**, checks 64 independent read offsets/content/checksums and rejected invalid/missing/cancelled requests. The test also checks futures remain completed after an owning reader drains pending work during destruction.

**Security:** no game assets, title-specific compression, RPF data, GPU file writes, shaders, leaked keys or external binaries. Offset+length overflow is rejected via subtraction after checking the offset; individual reads are capped to 4 MiB. The host OS filesystem and C++ standard threading primitives do the work.

**Performance boundary:** this is the portable fallback, not Windows DirectStorage, GPU decompression, NVMe GPU-to-memory transfer or Linux io_uring. No DirectStorage/io_uring API is called and no throughput claims are made. Those would be separate backend implementations behind the same validation and cancellation contract, requiring measured I/O workloads and legal input format knowledge.

Future tests: cancellation mid-stream, race/no-use-after-free, buffer residency, back-pressure under saturated queues, truncated file while active, checksum plus format-specific legal samples, Windows IOCP/DirectStorage vs Linux io_uring with verified kernel, and p95/p99 streaming stalls on target hardware. Keep a portable fallback for unsupported or remote drives.

FNV-1a is used as a fast, deterministic corruption-detection fixture checksum, **not a cryptographic content-authentication hash**. Production asset integrity will require authenticated manifests or a strong cryptographic hash where untrusted data enters the pipeline. The worker constructor also joins already started threads if another thread creation fails, preventing std::terminate on partial startup. Both Linux and Windows builds passed.

## Outstanding read admission budget (2026-11-24)

Reader now accepts a third constructor argument, requested-byte budget (default 128 MiB). Admission counts both queued and executing requests. Insufficient available credit returns Status::BudgetExceeded; callers must handle backpressure. Previously completed buffers retained by callers are not included. Credit is returned before the future is ready even on exceptions. Pre-cancelled valid requests return Cancelled before admission, with a further cancellation check after hashing. Reader::stats() provides a synchronized snapshot of queued/active requests, outstanding bytes, peak outstanding bytes and budget. Existing queue count and weighted fairness remain. Global budget is not a priority reservation or a cap on total RAM/VRAM. The standalone regression fixture uses 512 attempts and a 128 KiB test budget; the measured peak never exceeded the cap. This is not a measured application or SSD speedup.
