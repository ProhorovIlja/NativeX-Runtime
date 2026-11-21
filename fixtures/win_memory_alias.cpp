// Original Windows x64 virtual address reservation and shared mapping fixture.
// Does not read, translate, or execute any console executable.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <iostream>

struct Cleanup {
    void* reservation = nullptr;
    void* first = nullptr;
    void* second = nullptr;
    HANDLE mapping = nullptr;
    ~Cleanup() {
        if (first) UnmapViewOfFile(first);
        if (second) UnmapViewOfFile(second);
        if (mapping) CloseHandle(mapping);
        if (reservation) VirtualFree(reservation, 0, MEM_RELEASE);
    }
};

static int fail(const char* stage) {
    std::cerr << "win_memory_alias: " << stage
              << " (GetLastError=" << GetLastError() << ")\n";
    return 1;
}
static bool state(void* addr, DWORD expected) {
    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(addr, &info, sizeof(info)) == sizeof(info)
           && info.State == expected;
}

int main() {
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    if (sizeof(void*) != 8 || si.wProcessorArchitecture != PROCESSOR_ARCHITECTURE_AMD64)
        return fail("requires AMD64 Windows");
    const SIZE_T page = static_cast<SIZE_T>(si.dwPageSize);
    const SIZE_T granularity = static_cast<SIZE_T>(si.dwAllocationGranularity);
    if (page == 0 || granularity == 0 || granularity < page
        || granularity > 16 * 1024 * 1024 || page > granularity)
        return fail("unexpected page or allocation granularity");

    Cleanup c{};
    const SIZE_T reservation_bytes = 4 * page;
    c.reservation = VirtualAlloc(nullptr, reservation_bytes, MEM_RESERVE, PAGE_NOACCESS);
    if (!c.reservation) return fail("VirtualAlloc reserve");
    auto* base = static_cast<std::uint8_t*>(c.reservation);
    if (!state(base, MEM_RESERVE)) return fail("reservation state");
    if (!VirtualAlloc(base + page, 2 * page, MEM_COMMIT, PAGE_READWRITE))
        return fail("commit interior pages");
    if (!state(base, MEM_RESERVE) || !state(base + page, MEM_COMMIT)
        || !state(base + 2 * page, MEM_COMMIT) || !state(base + 3 * page, MEM_RESERVE))
        return fail("reservation and commitment boundaries");

    base[page] = 0x5Au;
    base[2 * page] = 0xC3u;
    DWORD old_protection = 0;
    if (!VirtualProtect(base + page, page, PAGE_READONLY, &old_protection)
        || old_protection != PAGE_READWRITE)
        return fail("change page protection");
    MEMORY_BASIC_INFORMATION protection_info{};
    if (VirtualQuery(base + page, &protection_info, sizeof(protection_info)) !=
            sizeof(protection_info)
        || protection_info.Protect != PAGE_READONLY)
        return fail("read-only protection query");
    if (!VirtualProtect(base + page, page, PAGE_READWRITE, &old_protection))
        return fail("restore page protection");

    if (!VirtualFree(base + 2 * page, page, MEM_DECOMMIT))
        return fail("decommit second page");
    if (!state(base + 2 * page, MEM_RESERVE)) return fail("decommit state");
    if (!VirtualAlloc(base + 2 * page, page, MEM_COMMIT, PAGE_READWRITE))
        return fail("recommit second page");
    // A decommitted anonymous page reinitialized on recommit must read zero.
    if (base[2 * page] != 0) return fail("recommit zero initialization");
    if (base[page] != 0x5Au) return fail("unrelated committed page changed");

    c.mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                                   static_cast<DWORD>(granularity), nullptr);
    if (!c.mapping) return fail("CreateFileMapping");
    c.first = MapViewOfFile(c.mapping, FILE_MAP_ALL_ACCESS, 0, 0, granularity);
    if (!c.first) return fail("first MapViewOfFile");
    c.second = MapViewOfFile(c.mapping, FILE_MAP_ALL_ACCESS, 0, 0, granularity);
    if (!c.second) return fail("second MapViewOfFile");
    if (c.first == c.second) return fail("shared views overlap unexpectedly");

    auto* a = static_cast<std::uint8_t*>(c.first);
    auto* b = static_cast<std::uint8_t*>(c.second);
    for (SIZE_T i = 0; i < granularity; ++i)
        a[i] = static_cast<std::uint8_t>((i * 29u + 13u) & 0xffu);
    for (SIZE_T i = 0; i < granularity; ++i)
        if (b[i] != static_cast<std::uint8_t>((i * 29u + 13u) & 0xffu))
            return fail("first-to-second alias consistency");
    for (SIZE_T i = 0; i < granularity; i += page)
        b[i] ^= 0xffu;
    for (SIZE_T i = 0; i < granularity; i += page)
        if (a[i] != static_cast<std::uint8_t>(((i * 29u + 13u) & 0xffu) ^ 0xffu))
            return fail("second-to-first alias consistency");

    std::cout << "{\"kind\":\"win32-reservation-and-alias\","
              << "\"reservation_state_verified\":true,"
              << "\"page_protection_verified\":true,"
              << "\"decommit_recommit_verified\":true,"
              << "\"two_mapping_views_coherent\":true,"
              << "\"bytes_checked\":" << granularity << ","
              << "\"xbox_xmem_tested\":false,\"gpu_coherency_tested\":false}\n";
    return 0;
}
