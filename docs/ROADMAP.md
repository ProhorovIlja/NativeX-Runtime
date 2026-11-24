# Engineering roadmap and acceptance gates

The roadmap applies to independently testable host infrastructure. Work is manual and on demand, with no scheduled scans or background tasks.

| Milestone | Focus | Evidence required |
| --- | --- | --- |
| M0 | Source and license baseline | Pinned source provenance, original fixtures and bounded diagnostics |
| M1 | PE32+ and host ABI | Ordinary/delayed imports, complete export tables, forwarders, TLS, unwind, and native source-built reference tests |
| M2 | Memory and streaming | Checked page allocation/aliasing, async I/O, bounded resources, lifetime and synchronization |
| M3 | Graphics | D3D12/Vulkan differential correctness, shader and texture validation, present pacing, hardware-specific capability tests |
| M4 | Host services | Native input, controller/audio support, persistence and configuration |
| M5 | Integration and optimization | Repeatable original application samples, measurable latency and throughput, physical GPU tests, documented failure paths |

Next: improve exact dependency and ABI verification, extend relocation/TLS tests with independently linked fixtures, and test memory/graphics interactions on actual supported Windows and Linux hardware. Upstream source review is performed when requested; license and provenance checks precede adoption.

Current results and limitations are in [STATE.md](STATE.md). Historic application-specific plans are not part of the public milestone requirements.
