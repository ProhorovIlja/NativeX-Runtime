// Source-built native Win32 loader fixture; never loads a retail title.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>

using BinaryOperation = int(__cdecl*)(int, int);

int wmain(int argc, wchar_t** argv) {
    if (argc != 2) {
        std::cerr << "usage: scarlett_abi_host_test <fixture DLL path>\n";
        return 2;
    }
    HMODULE dll = LoadLibraryW(argv[1]);
    if (!dll) {
        std::cerr << "failed to load source-built fixture DLL\n";
        return 1;
    }

    const auto add = reinterpret_cast<BinaryOperation>(GetProcAddress(dll, "ScarlettAdd"));
    const auto multiply_by_name = reinterpret_cast<BinaryOperation>(
        GetProcAddress(dll, "ScarlettMultiply"));
    const auto multiply_by_ordinal = reinterpret_cast<BinaryOperation>(
        GetProcAddress(dll, MAKEINTRESOURCEA(7)));
    const auto missing_name = GetProcAddress(dll, "MissingEntry");
    const auto missing_ordinal = GetProcAddress(dll, MAKEINTRESOURCEA(20));

    const bool passed =
        add && multiply_by_name && multiply_by_ordinal &&
        add(19, 23) == 42 &&
        multiply_by_name(6, 7) == 42 &&
        multiply_by_ordinal(6, 7) == 42 &&
        missing_name == nullptr && missing_ordinal == nullptr;

    const BOOL unloaded = FreeLibrary(dll);
    if (!passed || !unloaded) {
        std::cerr << "Windows loader fixture: import/export mismatch\n";
        return 1;
    }
    std::cout << "Windows x64 named/ordinal exports fixture PASS\n";
    return 0;
}
