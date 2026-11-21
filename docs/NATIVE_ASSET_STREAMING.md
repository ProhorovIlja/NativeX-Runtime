# Portable native Windows and Linux asset streaming baseline

This is a real, source-built asynchronous **worker-thread + filesystem** implementation in C++17, not merely a planned API. It reads deterministic synthetic local files on Windows and Linux with asynchronous futures, fixed-size request limits, cancellation, offset/EOF validation, FNV-1a integrity checks and clean queue draining on shutdown.

Files:
- src/native_asset_stream.hpp and src/native_asset_stream.cpp define bounded requests, status/error results and a native worker pool.
- fixtures/native_asset_stream_tests.cpp creates 2 MiB of its **own synthetic bytes**, checks 64 independent read offsets/content/checksums and rejected invalid/missing/cancelled requests. The test also checks futures remain completed after an owning reader drains pending work during destruction.

**Security:** no game assets, title-specific compression, RPF data, GPU file writes, shaders, leaked keys or external binaries. Offset+length overflow is rejected via subtraction after checking the offset; individual reads are capped to 4 MiB. The host OS filesystem and C++ standard threading primitives do the work.

**Performance boundary:** this is the portable fallback, not Windows DirectStorage, GPU decompression, NVMe GPU-to-memory transfer or Linux io_uring. No DirectStorage/io_uring API is called and no throughput claims are made. Those would be separate backend implementations behind the same validation and cancellation contract, requiring measured I/O workloads and legal input format knowledge.

Future tests: cancellation mid-stream, race/no-use-after-free, buffer residency, back-pressure under saturated queues, truncated file while active, checksum plus format-specific legal samples, Windows IOCP/DirectStorage vs Linux io_uring with verified kernel, and p95/p99 streaming stalls on target hardware. Keep a portable fallback for unsupported or remote drives.
