# Synthetic MSVC x64 hidden struct-return ABI test

The Windows x64 ABI has a subtle problem for C++ virtual methods returning aggregates. A 16-byte `AllocationInfo`-shaped object is returned via a caller-allocated **hidden result pointer** before the `this` pointer in the raw call convention. Incorrect function signatures can overwrite the object vtable instead of filling the result structure.

This source-built C++ test creates a mock virtual interface with 25 dummy methods and a **synthetic** slot 25, then compares:

1. a conventional C++ virtual `AllocationInfo allocation_info(...)` call, and
2. a raw vtable-entry function pointer using `void (*)(AllocationInfo* out, DeviceInterface* self, ...)`.

Both must return exactly the expected size and alignment, increment the same call counter and leave guard values on both sides of the object unchanged. The test is *only compiled for Windows x64* and is intended for MSVC. No real Xbox GDK interface definition, console DLL or title is included.

**Research motivation:** [XCL e555b87](https://github.com/ivall/xcl/commit/e555b87c07482c139c18baf0c20effa51b4b3938) reports a title failing at a console `GetResourceAllocationInfo` call when a return buffer was confused with a COM object pointer. Their method slot numbering and layout are title-specific observations; our 25 is an artificial slot and is **not** claimed to describe a universal console vtable.

The test does not resolve missing D3D12.X graphics command records or establish game compatibility. All source is original, without importing XCL's GPL-2.0-only implementation.
