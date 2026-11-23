#include "native_asset_texture_publish.hpp"
#include <limits>

namespace scarlett::texturestream {
namespace {
std::size_t bytes_per_block(texture::Format format) noexcept {
    switch(format){
        case texture::Format::Bc1:
        case texture::Format::Bc4:return 8u;
        case texture::Format::Bc3:
        case texture::Format::Bc5:
        case texture::Format::Bc7Mode6:
        case texture::Format::Bc7All:return 16u;
    }
    return 0;
}
} // namespace
Published publish(const stream::Result& source,texture::Format format,
                  std::uint32_t width,std::uint32_t height,
                  std::size_t pitch) {
    if(source.status!=stream::Status::Ok)return {Status::ReadFailed};
    const auto block_bytes=bytes_per_block(format);
    if(block_bytes==0)return {Status::InvalidFormat};
    if(width==0||height==0||width>8192||height>8192)
        return {Status::InvalidDimensions};
    const std::size_t rowbytes=static_cast<std::size_t>(width)*4u;
    const auto total=rowbytes*static_cast<std::size_t>(height);
    if(total>MaxPublishedRgbaBytes)return {Status::OutputBudgetExceeded};
    const auto cols=(static_cast<std::size_t>(width)+3u)/4u;
    const auto rows=(static_cast<std::size_t>(height)+3u)/4u;
    const auto min_pitch=cols*block_bytes;
    if(pitch<min_pitch ||
       (rows>1u && pitch>
        (std::numeric_limits<std::size_t>::max()-min_pitch)/(rows-1u)))
        return {Status::InvalidStride};
    const auto expected=(rows-1u)*pitch+min_pitch;
    if(source.bytes.size()!=expected)
        return {Status::CompressedByteCountMismatch};
    // A non-cryptographic consistency check for Reader-to-decoder handoff,
    // NOT authentication of downloads, publisher signatures or mod safety.
    if(source.checksum!=stream::fnv1a64(source.bytes))
        return {Status::InputChecksumMismatch};
    Published output{};
    output.rgba.resize(total);
    const auto result=texture::decode(
        {source.bytes.data(),source.bytes.size(),pitch,width,height},
        {output.rgba.data(),output.rgba.size(),rowbytes},format);
    if(result!=texture::Status::Ok) {
        // Never expose any partially decoded pixels as a renderable texture.
        return {Status::DecodeFailed,result};
    }
    output.status=Status::Ready;
    output.decode_status=result;
    output.width=width;output.height=height;
    return output;
}
const char* name(Status s) noexcept {
    switch(s){
        case Status::Ready:return "ready";
        case Status::ReadFailed:return "read-failed";
        case Status::InvalidFormat:return "invalid-format";
        case Status::InvalidDimensions:return "invalid-dimensions";
        case Status::InvalidStride:return "invalid-stride";
        case Status::OutputBudgetExceeded:return "output-budget-exceeded";
        case Status::CompressedByteCountMismatch:return "compressed-size-mismatch";
        case Status::InputChecksumMismatch:return "checksum-mismatch";
        case Status::DecodeFailed:return "decode-failed";
    }
    return "invalid";
}
} // namespace scarlett::texturestream
