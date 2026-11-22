#include "native_bc_texture.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
namespace scarlett::texture {
namespace {
constexpr std::size_t MaxDimension=8192;
struct Rgba {std::uint8_t r,g,b,a;};
std::uint16_t u16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0]|(static_cast<unsigned>(p[1])<<8u));
}
std::uint32_t u32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0])|
           (static_cast<std::uint32_t>(p[1])<<8u)|
           (static_cast<std::uint32_t>(p[2])<<16u)|
           (static_cast<std::uint32_t>(p[3])<<24u);
}
Rgba from565(std::uint16_t v) noexcept {
    const unsigned r=(v>>11u)&31u, g=(v>>5u)&63u, b=v&31u;
    return {static_cast<std::uint8_t>((r<<3u)|(r>>2u)),
            static_cast<std::uint8_t>((g<<2u)|(g>>4u)),
            static_cast<std::uint8_t>((b<<3u)|(b>>2u)),255};
}
Rgba combine(Rgba a,Rgba b,unsigned na,unsigned nb,unsigned denominator) noexcept {
    return {static_cast<std::uint8_t>((na*a.r+nb*b.r)/denominator),
            static_cast<std::uint8_t>((na*a.g+nb*b.g)/denominator),
            static_cast<std::uint8_t>((na*a.b+nb*b.b)/denominator),255};
}
std::array<Rgba,4> color_table(const std::uint8_t* block,bool bc3) noexcept {
    const auto c0=u16(block),c1=u16(block+2);
    std::array<Rgba,4> entries{};
    entries[0]=from565(c0);entries[1]=from565(c1);
    if(c0>c1 || bc3){
        entries[2]=combine(entries[0],entries[1],2,1,3);
        entries[3]=combine(entries[0],entries[1],1,2,3);
    }else{
        entries[2]=combine(entries[0],entries[1],1,1,2);
        entries[3]={0,0,0,0};
    }
    return entries;
}
std::array<std::uint8_t,8> alpha_table(const std::uint8_t* block) noexcept {
    std::array<std::uint8_t,8> out{};
    out[0]=block[0];out[1]=block[1];
    const unsigned a0=out[0],a1=out[1];
    if(a0>a1){
        for(unsigned index=2;index<8;index++)
            out[index]=static_cast<std::uint8_t>(
                ((8u-index)*a0+(index-1u)*a1)/7u);
    }else{
        for(unsigned index=2;index<6;index++)
            out[index]=static_cast<std::uint8_t>(
                ((6u-index)*a0+(index-1u)*a1)/5u);
        out[6]=0;out[7]=255;
    }
    return out;
}
bool required(std::size_t rows,std::size_t pitch,std::size_t row_bytes,
              std::size_t& bytes) noexcept {
    if(rows==0 || pitch<row_bytes)return false;
    if(rows-1 && pitch>(std::numeric_limits<std::size_t>::max()-row_bytes)/(rows-1))
        return false;
    bytes=(rows-1)*pitch+row_bytes;
    return true;
}
}
Status decode(const Surface& src,Output dst,Format kind) noexcept {
    if(!src.blocks || !dst.rgba)return Status::NullPointer;
    if(!src.width || !src.height ||
       src.width>MaxDimension || src.height>MaxDimension)
        return Status::InvalidDimensions;
    if(kind!=Format::Bc1 && kind!=Format::Bc3)
        return Status::UnsupportedFormat;
    const std::size_t width=src.width,height=src.height;
    const std::size_t block_columns=(width+3u)/4u,block_rows=(height+3u)/4u;
    const std::size_t block_bytes=kind==Format::Bc3?16u:8u;
    std::size_t required_src=0,required_dest=0;
    if(!required(block_rows,src.block_pitch,block_columns*block_bytes,
                 required_src) ||
       !required(height,dst.pitch,width*4u,required_dest))
        return Status::InvalidStride;
    if(src.block_bytes<required_src || dst.bytes<required_dest)
        return Status::BufferTooShort;
    // Full prevalidation before any output byte changes, including row padding.
    // The caller guarantees that compressed source and output buffers do not
    // overlap and remain owned for the entire synchronous conversion call.
    for(std::size_t by=0;by<block_rows;by++){
        for(std::size_t bx=0;bx<block_columns;bx++){
            const auto* block=src.blocks+by*src.block_pitch+bx*block_bytes;
            const auto* colors=block+(kind==Format::Bc3?8u:0u);
            const auto table=color_table(colors,kind==Format::Bc3);
            const auto indices=u32(colors+4);
            std::array<std::uint8_t,8> alpha{};
            std::uint64_t alpha_bits=0;
            if(kind==Format::Bc3){
                alpha=alpha_table(block);
                for(std::size_t k=0;k<6;k++)
                    alpha_bits|=static_cast<std::uint64_t>(block[k+2])<<(8u*k);
            }
            for(std::size_t py=0;py<4;py++){
                const std::size_t y=by*4u+py;
                if(y>=height)break;
                for(std::size_t px=0;px<4;px++){
                    const std::size_t x=bx*4u+px;
                    if(x>=width)break;
                    const unsigned index=static_cast<unsigned>(py*4u+px);
                    const auto pixel=table[(indices>>(2u*index))&3u];
                    auto* out=dst.rgba+y*dst.pitch+x*4u;
                    out[0]=pixel.r;out[1]=pixel.g;out[2]=pixel.b;
                    out[3]=kind==Format::Bc3
                         ?alpha[(alpha_bits>>(3u*index))&7u]
                         :pixel.a;
                }
            }
        }
    }
    return Status::Ok;
}
const char* status_name(Status status) noexcept {
    switch(status){
        case Status::Ok:return "ok";
        case Status::NullPointer:return "null-pointer";
        case Status::InvalidDimensions:return "invalid-dimensions";
        case Status::InvalidStride:return "invalid-stride";
        case Status::BufferTooShort:return "buffer-too-short";
        case Status::UnsupportedFormat:return "unsupported-format";
    }
    return "unknown";
}
} // namespace scarlett::texture
