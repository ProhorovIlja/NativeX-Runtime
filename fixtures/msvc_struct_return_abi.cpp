// Original MSVC x64 C++ virtual-method struct-return ABI regression fixture.
// The 25 prior slots are artificial; this is NOT an Xbox vtable description.
#include <cstdint>
#include <iostream>

struct ResourceDescription { std::uint64_t bytes; };
struct AllocationInfo { std::uint64_t size, alignment; };
static_assert(sizeof(AllocationInfo)==16,"test needs a 16-byte aggregate");

struct DeviceInterface {
    virtual std::uint64_t reserved_00() noexcept=0;
    virtual std::uint64_t reserved_01() noexcept=0;
    virtual std::uint64_t reserved_02() noexcept=0;
    virtual std::uint64_t reserved_03() noexcept=0;
    virtual std::uint64_t reserved_04() noexcept=0;
    virtual std::uint64_t reserved_05() noexcept=0;
    virtual std::uint64_t reserved_06() noexcept=0;
    virtual std::uint64_t reserved_07() noexcept=0;
    virtual std::uint64_t reserved_08() noexcept=0;
    virtual std::uint64_t reserved_09() noexcept=0;
    virtual std::uint64_t reserved_10() noexcept=0;
    virtual std::uint64_t reserved_11() noexcept=0;
    virtual std::uint64_t reserved_12() noexcept=0;
    virtual std::uint64_t reserved_13() noexcept=0;
    virtual std::uint64_t reserved_14() noexcept=0;
    virtual std::uint64_t reserved_15() noexcept=0;
    virtual std::uint64_t reserved_16() noexcept=0;
    virtual std::uint64_t reserved_17() noexcept=0;
    virtual std::uint64_t reserved_18() noexcept=0;
    virtual std::uint64_t reserved_19() noexcept=0;
    virtual std::uint64_t reserved_20() noexcept=0;
    virtual std::uint64_t reserved_21() noexcept=0;
    virtual std::uint64_t reserved_22() noexcept=0;
    virtual std::uint64_t reserved_23() noexcept=0;
    virtual std::uint64_t reserved_24() noexcept=0;
    virtual AllocationInfo allocation_info(std::uint32_t visible_mask,
                                           std::uint32_t count,
                                           const ResourceDescription* desc) noexcept=0;
};
struct FakeDevice final : DeviceInterface {
    std::uint64_t guard_before=0x1122334455667788ull;
    std::uint64_t calls=0;
    std::uint64_t guard_after=0x8877665544332211ull;
    std::uint64_t reserved_00() noexcept override { return 0u; }
    std::uint64_t reserved_01() noexcept override { return 1u; }
    std::uint64_t reserved_02() noexcept override { return 2u; }
    std::uint64_t reserved_03() noexcept override { return 3u; }
    std::uint64_t reserved_04() noexcept override { return 4u; }
    std::uint64_t reserved_05() noexcept override { return 5u; }
    std::uint64_t reserved_06() noexcept override { return 6u; }
    std::uint64_t reserved_07() noexcept override { return 7u; }
    std::uint64_t reserved_08() noexcept override { return 8u; }
    std::uint64_t reserved_09() noexcept override { return 9u; }
    std::uint64_t reserved_10() noexcept override { return 10u; }
    std::uint64_t reserved_11() noexcept override { return 11u; }
    std::uint64_t reserved_12() noexcept override { return 12u; }
    std::uint64_t reserved_13() noexcept override { return 13u; }
    std::uint64_t reserved_14() noexcept override { return 14u; }
    std::uint64_t reserved_15() noexcept override { return 15u; }
    std::uint64_t reserved_16() noexcept override { return 16u; }
    std::uint64_t reserved_17() noexcept override { return 17u; }
    std::uint64_t reserved_18() noexcept override { return 18u; }
    std::uint64_t reserved_19() noexcept override { return 19u; }
    std::uint64_t reserved_20() noexcept override { return 20u; }
    std::uint64_t reserved_21() noexcept override { return 21u; }
    std::uint64_t reserved_22() noexcept override { return 22u; }
    std::uint64_t reserved_23() noexcept override { return 23u; }
    std::uint64_t reserved_24() noexcept override { return 24u; }
    AllocationInfo allocation_info(std::uint32_t mask,std::uint32_t count,
                                   const ResourceDescription* desc) noexcept override {
        ++calls;
        return {desc[0].bytes + static_cast<std::uint64_t>(mask) + count,65536u};
    }
};
static bool guards_ok(const FakeDevice& d) {
    return d.guard_before==0x1122334455667788ull &&
           d.guard_after==0x8877665544332211ull;
}
static int fail(const char* why) { std::cerr<<why<<'\n';return 1; }

int main() {
    if(sizeof(void*)!=8)return fail("x64-only fixture");
    FakeDevice device{};
    DeviceInterface* iface=&device;
    const ResourceDescription desc{1073741824ull};
    const AllocationInfo high_level=iface->allocation_info(7,1,&desc);
    if(high_level.size!=1073741832ull || high_level.alignment!=65536 ||
       device.calls!=1 || !guards_ok(device))
        return fail("ordinary MSVC x64 virtual struct return failed");

#ifdef _MSC_VER
    // In the Microsoft x64 C++ ABI a 16-byte return is written via a hidden
    // caller-provided buffer before the object ("this") argument.
    // This explicitly tests the raw ABI by invoking a compiler-generated
    // *synthetic* method through its vtable. No Xbox executable is loaded.
    using StructReturnAbi = void (*)(AllocationInfo*,DeviceInterface*,
                                     std::uint32_t,std::uint32_t,
                                     const ResourceDescription*) noexcept;
    void** slots=*reinterpret_cast<void***>(iface);
    if(!slots || !slots[25])return fail("missing synthetic vtable slot 25");
    const auto call=reinterpret_cast<StructReturnAbi>(slots[25]);
    AllocationInfo raw{0,0};
    call(&raw,iface,7,1,&desc);
    if(raw.size!=high_level.size || raw.alignment!=high_level.alignment ||
       device.calls!=2 || !guards_ok(device))
        return fail("hidden struct-return parameter or object pointer ABI mismatch");
    std::cout<<"{\"kind\":\"msvc-x64-struct-return-abi\","
               "\"synthetic_slot\":25,\"hidden_result_parameter_verified\":true,"
               "\"vtable_canaries_intact\":true,"
               "\"console_abi_verified\":false,\"game_tested\":false}\n";
#else
    // Other compilers may choose a different calling convention. Do not
    // pretend a GCC/Clang Windows ABI check was performed.
    std::cout<<"SKIP: MSVC-specific raw vtable ABI test\n";
    return 77;
#endif
    return 0;
}
