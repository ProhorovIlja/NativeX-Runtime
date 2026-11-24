# Native-first architecture

NativeX Runtime is a set of independent Windows/Linux x64 interoperability research components. It favors ordinary host processes, supported host APIs and source-built test cases. Compatible instruction sets alone do not establish OS, ABI, memory or graphics compatibility.

| Component | Evidence goal |
| --- | --- |
| PE32+ intake | Bounds, sections, imports, exports, TLS, exception and relocation metadata |
| Dependency and ABI analysis | Explicit provider contracts, forwarder validation, exact host ABI and failure behavior |
| Memory | Page allocation, aliasing, lifetime, synchronization and checked CPU/GPU backing |
| Graphics | D3D12 and Vulkan resource, queue, barrier, shader and present correctness |
| I/O | Read bounds, async completion, error propagation and streaming budgets |
| Input and audio | Native device APIs, deterministic event processing, verified audio buffers |
| Persistence and performance | Atomic recovery, frame pacing, resource and timing measurements |

Use the [native host target](NATIVE_TARGET.md), [manual roadmap](ROADMAP.md), [PE tools](NATIVE_PE_TOOLS.md) and [state](STATE.md) to distinguish implemented components from proposed integration. Retained dated documentation records earlier experiments, not guaranteed runtime compatibility.
