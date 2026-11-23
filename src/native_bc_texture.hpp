// Original safe portable BC1/BC3/BC4/BC5 and BC7 modes (reference) 4x4 block -> RGBA8 CPU reference fallback.
// Linear D3D block raster only. NOT proprietary Xbox GPU swizzle or full 8-mode BC7 decoding.
#pragma once
#include <cstddef>
#include <cstdint>
namespace scarlett::texture {
enum class Format : std::uint8_t { Bc1=1, Bc3=3, Bc4=4, Bc5=5, Bc7Mode6=6, Bc7All=7 };
enum class Status : std::uint8_t {
    Ok,NullPointer,InvalidDimensions,InvalidStride,BufferTooShort,UnsupportedFormat,UnsupportedBlockMode
};
struct Surface {
    const std::uint8_t* blocks=nullptr;
    std::size_t block_bytes=0;
    std::size_t block_pitch=0; // compressed BYTES per row of 4x4 blocks
    std::uint32_t width=0;     // actual visible texel width (need not be 4-aligned)
    std::uint32_t height=0;
};
struct Output {
    std::uint8_t* rgba=nullptr;
    std::size_t bytes=0;
    std::size_t pitch=0;
};
Status decode(const Surface& source, Output dest, Format format) noexcept;
const char* status_name(Status status) noexcept;
} // namespace scarlett::texture
