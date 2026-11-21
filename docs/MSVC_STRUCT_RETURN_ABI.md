# Synthetic MSVC x64 hidden struct-return ABI test

The Windows x64 ABI has a subtle problem for C++ virtual methods returning aggregates. In this generated MSVC x64 virtual method, a 16-byte `AllocationInfo`-shaped object is returned via a caller-allocated **hidden result pointer after `this`** in the raw call signature. This order was established by executable CI, not assumed from an Xbox method name. Incorrect function signatures can overwrite the object vtable instead of filling the result structure.

This source-built C++ test creates a mock virtual interface with 25 dummy methods and a **synthetic** slot 25, then compares:

1. a conventional C++ virtual `AllocationInfo allocation_info(...)` call, and
2. a raw vtable-entry function pointer using an explicit raw function pointer signature and checking its result against the compiler-generated C++ virtual call.

Both must return exactly the expected size and alignment, increment the same call counter and leave guard values on both sides of the object unchanged. The test is *only compiled for Windows x64* and is intended for MSVC. No real Xbox GDK interface definition, console DLL or title is included.

**Research motivation:** [XCL e555b87](https://github.com/ivall/xcl/commit/e555b87c07482c139c18baf0c20effa51b4b3938) reports a title failing at a console `GetResourceAllocationInfo` call when a return buffer was confused with a COM object pointer. Their method slot numbering and layout are title-specific observations; our 25 is an artificial slot and is **not** claimed to describe a universal console vtable.

The test does not resolve missing D3D12.X graphics command records or establish game compatibility. All source is original, without importing XCL's GPL-2.0-only implementation.


**ABI verification note:** the first attempt using an `out,self` raw signature failed the MSVC Windows CTest (without crashing). A follow-up experiment tests `self,out` ordering and emits both raw and C++ values on mismatch. The follow-up test using `self,out` ordering **passed** Microsoft Visual C++ on the public Windows x64 runner ([run 44954902495](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/actions/runs/44954902495)): native virtual dispatch and raw vtable-call results matched, with the object canaries intact. The failed `out,self` experiment remains documented so future code does not repeat the same mistaken assumption. This is precisely why console-to-PC method signatures must not be assumed from a method name.
