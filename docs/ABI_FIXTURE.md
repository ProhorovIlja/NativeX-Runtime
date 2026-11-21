# Windows x64 named and ordinal export fixture

A source-built DLL exports two simple C ABI functions, `ScarlettAdd` and `ScarlettMultiply`. The host program uses `LoadLibraryW` and `GetProcAddress` to load one export by name and one by ordinal (`#7`). Invalid names and ordinals must be rejected. CTest asserts the results.

This demonstrates the **desktop Windows** side of a compatibility boundary. Xbox GDK return conventions, console PE subsystem and binary import tables still require further controlled research. This test does **not** run an Xbox executable or emulate Game OS.

The fixture is built only for Windows x64. Linux tests stay focused on the portable read-only PE32+ parser and Python tooling.

Technical follow-up: extend the fixture to test delay imports, TLS callbacks, PE relocations, forwarding and error propagation, with independent, source-built inputs. Related task: [issue #2](https://github.com/ProhorovIlja/Scarlett-Windows-Compat/issues/2).
