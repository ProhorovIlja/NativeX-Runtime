// Original BCn compressed-file-read -> complete RGBA8 publication contract.
// Caller waits for the existing native asset Reader. No GPU, RPF9, game data.
#pragma once
#include "native_asset_stream.hpp"
#include "native_bc_texture.hpp"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace scarlett::texturestream {
enum class Status : std::uint8_t {
    Ready,
    ReadFailed,
    InvalidFormat,
    InvalidDimensions,
    InvalidStride,
    OutputBudgetExceeded,
    CompressedByteCountMismatch,
    InputChecksumMismatch,
    DecodeFailed
};
constexpr std::size_t MaxPublishedRgbaBytes=32u*1024u*1024u;
struct Published {
    Status status=Status::ReadFailed;
    scarlett::texture::Status decode_status=scarlett::texture::Status::Ok;
    std::uint32_t width=0;
    std::uint32_t height=0;
    // Contains a complete valid row-major image ONLY when status == Ready.
    std::vector<std::uint8_t> rgba{};
};
// Synchronous CPU stage after native Reader::submit(req).get(). Surface pitch
// describes the compressed source's 4x4 block-row storage (not image row).
// A result that is truncated, cancelled, invalid, checksum-inconsistent or
// uses unsupported compressed block modes is NEVER published.
Published publish(const scarlett::stream::Result& source,
                  scarlett::texture::Format format,
                  std::uint32_t width,std::uint32_t height,
                  std::size_t compressed_block_row_bytes);
const char* name(Status status) noexcept;
} // namespace scarlett::texturestream
