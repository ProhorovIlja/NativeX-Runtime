# Native Win32 IOCP streaming storage fixture

Original C++17 `fixtures/native_win_iocp_disk.cpp` verifies actual Windows kernel **overlapped file I/O and completion ports** with independently checked content (no game asset files). It creates a unique temporary file, opens it with `FILE_FLAG_OVERLAPPED` and `FILE_FLAG_DELETE_ON_CLOSE`, associates the handle with a real `CreateIoCompletionPort`, issues eight 4 KiB writes with individual OVERLAPPED offsets and waits for all eight completions (order-independent) via `GetQueuedCompletionStatus`. The test then posts eight reads with independent offsets, verifies every byte against the original payload and a checksum, and checks file size. Data and buffers are never freed before all matching completions arrive.

The fixture tolerates both immediate `WriteFile`/`ReadFile` success and `ERROR_IO_PENDING`, and fails on unknown or duplicate completion tokens, wrong transfer sizes or mismatched data. It uses bounded waits and CTest timeout, and does not copy encrypted/commercial game files.

### What this means

Verified native Windows x64 asynchronous disk queue semantics constitute a usable small building block for a game asset streaming subsystem. The result does **not** prove low latency, DirectStorage compatibility, GDeflate decompression, NVMe hardware throughput, unbuffered/aligned I/O, high-queue-depth streaming or GTA VI storage compatibility. DirectStorage should only be integrated after a real asset layout and renderer are understood. We need actual disk/queue performance tests (including p95/p99 latency and cancellation) before selecting a production implementation. Existing portable `native_asset_stream` tests cover a different, bounded std::ifstream worker path.

### Sources

- [Microsoft CreateIoCompletionPort](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-createiocompletionport)
- [Microsoft GetQueuedCompletionStatus](https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getqueuedcompletionstatus)
- [Microsoft overlapped I/O](https://learn.microsoft.com/en-us/windows/win32/sync/synchronization-and-overlapped-input-and-output)
- [Microsoft DirectStorage](https://github.com/microsoft/DirectStorage), a **separate Windows API and SDK**, not interchangeable with IOCP.

No additional libraries, drivers, generated background services or paid runners are needed.
