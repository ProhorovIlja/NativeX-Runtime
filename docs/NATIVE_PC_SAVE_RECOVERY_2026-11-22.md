# Native Windows/Linux two-slot save snapshot recovery (original C++17 fixture)

Updated 2026-11-22. Implementation: `src/native_save_slots.{hpp,cpp}` and `fixtures/native_save_slots_tests.cpp`. This is an independent, source-built *transaction primitive* for future PC autosave work, **not** a reverse-engineered GTA VI/Rockstar save format or a working in-game autosave feature.

## Contract and layout

- Two independent committed slots in one **private, trusted** per-user directory: `slot-a.scp` / `slot-b.scp`. A commit alternates away from the highest valid generation to preserve the previous one. A temporary staging file `save-stage.tmp` is fully written and synchronized before replacing the target. After a process restart, loader picks the highest generation with a valid header/length/hash; a corrupt newest slot falls back to the last intact generation. Two corrupt slots fail closed rather than silently producing invented progress.
- Original binary record: ASCII magic `SCSAVE01` (8 bytes), little-endian version `u32=1`, reserved `u32=0`, generation `u64`, payload length `u64`, checksum FNV-1a 64 (`u64`) over header bytes 0–31 and payload, payload bytes at offset 40. Max **2 MiB** per snapshot. This checksum detects accidental corruption, **not intentional tampering**, encryption, digital signatures or piracy/DRM compatibility.
- Optimistic API uses `expected_generation`: a mismatch is rejected before writing; a caller must reload after any ambiguous error. Only **one writer in one process** is supported; no locking or arbitration across processes, no filesystem watcher, daemon or scheduled background task.
- Windows: uses `CreateFileW`, `WriteFile`, `FlushFileBuffers`, then same-directory `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`. Linux: uses `open/write/fsync/rename`, followed by `fsync` on the containing directory. A failed synchronization or replacement reports an I/O error; the caller must reload because an error after publishing may leave a visible new generation.
- Synthetic **fault injection** at `BeforePublish` and `AfterPublish` checks expected observable recovery, including safe handling of an uncertain completion. It does not kill processes, crash the kernel, detach disks or prove persistence after actual loss of power.

## I/O error fail-closed correction (November 22)

A safety bug was identified in the first version's slot selection. When slot A was valid but slot B's metadata or file data was **unreadable** (`IoError`, not merely checksum-corrupt), the original selector could accept A as the latest known save. That is unsafe: B might contain an unreadably newer generation, and the caller could trust an obsolete progress state or start a commit using an incomplete generation picture.

The selector now gives **`IoError` priority over every apparently valid peer** before choosing any generation. For both `load` and `commit`, an ambiguous unreadable slot fails closed. The existing more permissive behavior for checksum-corrupt slots is retained: a physically readable **corrupt** newest slot can still fall back to a verified previous generation, under the documented two-slot recovery contract.

The new independent Windows/Linux test first commits generation 1, replaces the currently absent slot B with a directory to force a deterministic `file_size` error, verifies that `load` returns `IoError` with a reset destination and that `commit` refuses to publish. It then removes the invalid directory, commits generation 2, repeats the experiment in the opposite direction with slot A unreadable and generation-2 slot B intact, verifies fail-closed again, restores the missing old slot and confirms generation 3 can be committed and recovered. No OS permission change, administrative privilege or artificial hardware failure is required.

## Evidence and acceptance

The deterministic Linux/Windows CTest checks missing directory, round-trip/reopen, stale write rejection, interruption before/after atomic publish, stage cleanup, corrupt-newest rollback, both-slots-corrupt fail-closed, empty snapshot, maximum-size payload, oversized payload rejection, and truncated record rejection. It writes **only freshly generated synthetic bytes to OS temporary directories**, removed at test exit. No game saves, title files or cloud sync accounts are accessed.

The two slots are a deliberately small *reference protocol*, not a replacement for robust databases. For larger state or synchronized multi-file transactions, evaluate [SQLite WAL and `synchronous=FULL/EXTRA`](https://www.sqlite.org/pragma.html), its platform-specific flush assumptions and backup/recovery behavior; introducing SQLite would require a specific licensing and binary-distribution decision. See also [SQLite's atomic commit analysis](https://www.sqlite.org/atomiccommit.html), [Microsoft MoveFileExW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw) and [Linux filesystem sync guidance](https://man7.org/linux/man-pages/man2/fsync.2.html).

## Limitations blocking production status

This model assumes a trustworthy directory, stable single-process ownership, no symlink attack, reliable replacement semantics on a local filesystem and a stable machine/drive. We have **not** tested directory creation durability (new directory's parent may also need syncing), torn metadata, actual power-cut hardware, antivirus locks, removable/cloud/network filesystems, multiple writers, OS suspend, storage-full conditions or machine-specific SSD caching behavior. Windows directory-entry persistence is not treated as formally guaranteed by `MOVEFILE_WRITE_THROUGH`. High-frequency autosave also needs scheduling policy, rate limiting, UI integration and actual game-state serialization. Do not claim crash-proof saves in GTA VI until real end-to-end title save/load tests exist.

The source-derived GTA VI project priority is unchanged: **first native Windows single-player compatibility, reliability and genuine game saves; Linux native second; cooperative game state synchronization and mission skip only after the port is demonstrably functional.**
