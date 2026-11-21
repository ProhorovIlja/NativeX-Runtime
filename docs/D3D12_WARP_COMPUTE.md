# Source-built D3D12/WARP compute/readback fixture

A **native Windows x64 Direct3D 12** program compiles a short, synthetic HLSL shader model 5.0 compute kernel through the Windows D3DCompiler, runs 64 threads in a single dispatch on the software WARP adapter, transitions a UAV resource into a copy source, copies the result to a READBACK heap, signals a queue fence and compares all 64 resulting 32-bit values on the CPU.

This verifies much more than a device/queue creation smoke test:

- source shader compiled by desktop Windows D3DCompiler;
- desktop D3D12 compute pipeline and root signature;
- GPU virtual address exposed through a root UAV descriptor;
- synthetic compute dispatch;
- resource barrier and GPU-to-CPU readback;
- fence synchronization and deterministic CPU output comparison.

**Scope restriction:** It does not exercise HLSL WaveReadLaneAt, Shader Model 6, RDNA2 ISA, translated Xbox shaders, D3D12.X/Scarlett APIs, physical GPU drivers or any commercial game. A pass is a desktop D3D12/WARP compute result only. No paid GPU runners or copyrighted inputs are used.

This test exits 77 (CTest skip) when the host lacks WARP/D3D12. Once a WARP device exists, failures in compilation, pipeline creation, fence, compute output and readback are **failures**, never silent skips.
