// Original portable NV12->BGRA8 CPU fallback (two studio-range BT matrices).
// No decoder, player, codec bitstream, GPU shader or proprietary game data.
#pragma once
#include <cstddef>
#include <cstdint>
namespace scarlett::video {
enum class Matrix : std::uint8_t { Bt601Limited, Bt709Limited };
enum class Status : std::uint8_t {
    Ok, NullBuffer, InvalidDimensions, InvalidStride, BufferTooShort, UnknownMatrix
};
struct Nv12 {
    const std::uint8_t* y=nullptr;
    const std::uint8_t* uv=nullptr;
    std::size_t y_bytes=0;
    std::size_t uv_bytes=0;
    std::size_t y_pitch=0;
    std::size_t uv_pitch=0;
    std::uint32_t width=0;
    std::uint32_t height=0;
};
struct Bgra {
    std::uint8_t* pixels=nullptr;
    std::size_t bytes=0;
    std::size_t pitch=0;
};
constexpr std::uint32_t MaxDimension=8192;
Status convert(const Nv12& input,Bgra output,Matrix matrix) noexcept;
const char* status_name(Status status) noexcept;
} // namespace scarlett::video
