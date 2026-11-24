// Original portable CPU staging layout for D3D12-style buffer-to-texture copies.
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace scarlett::upload {
enum class Format { Rgba8, Bc1, Bc3, Bc4, Bc5, Bc7 };
enum class Status { Ready, InvalidDescription, BudgetExceeded, InvalidSource };
struct Description { Format format=Format::Rgba8; std::uint32_t width=0,height=0,mips=1,layers=1; };
struct Footprint {
    std::uint32_t mip=0, layer=0, width=0,height=0;
    std::size_t offset=0,row_bytes=0,row_pitch=0,rows=0;
};
struct Plan { Status status=Status::InvalidDescription; std::size_t bytes=0; std::vector<Footprint> subresources; };
struct View { const std::uint8_t* data=nullptr; std::size_t size=0,pitch=0; };
struct Staged { Plan plan; std::vector<std::uint8_t> bytes; };
constexpr std::size_t MaxStagingBytes=256u*1024u*1024u;
// Conservative full-row allocation, one plane, 2D only. The actual device's
// GetCopyableFootprints remains the authority for GPU resource creation.
Plan plan(Description description,std::size_t budget=MaxStagingBytes);
// Sources ordered by layer, then mip. Copy only meaningful bytes per row.
// Caller guarantees each pointer denotes a readable buffer of the stated size.
// Reject all invalid views before allocating/copying any payload.
Staged stage(Description description,const std::vector<View>& sources,
             std::size_t budget=MaxStagingBytes);
} // namespace scarlett::upload
