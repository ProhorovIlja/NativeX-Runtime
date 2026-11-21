# Contributing

We prioritize reproducibility and clean-room evidence, not headline compatibility percentages.

Changes must contain a clear test (synthetic fixture or appropriately licensed example), expected vs actual behavior, minimal logs and exact upstream references. All patches should be reviewable without game files.

Pull requests must not include commercial title code, Xbox proprietary binaries, secrets, copyrighted shader blobs or large dumps. Prefer pure Python/C++17 source and standard-library tooling. Check upstream source license and dependencies before reuse.

Issue categories: loader/ABI, graphics/D3D12.X, RDNA2/waves, memory, input/audio, runtime services, performance, tests and upstream audit.

All claims must use **one** evidence level: author claim; source inspected; fixture tested; game tested; independent reproduction. CI on a synthetic fixture does not make a retail game runnable.