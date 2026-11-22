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
std::uint64_t index_bits48(const std::uint8_t* block) noexcept {
    std::uint64_t bits=0;
    for(std::size_t i=0;i<6;i++)
        bits|=static_cast<std::uint64_t>(block[i+2u])<<(8u*i);
    return bits;
}
// BC7's fixed 128-bit little-endian block; only reads statically bounded
// 1..7-bit fields for mode 6. No pointer casts, aliasing or word shifts by 64.
unsigned bc7_bits(const std::uint8_t* p,unsigned start,unsigned count) noexcept {
    unsigned value=0;
    for(unsigned i=0;i<count;i++)
        value|=static_cast<unsigned>((p[(start+i)/8u]>>((start+i)%8u))&1u)<<i;
    return value;
}
constexpr std::array<unsigned,16> Bc7Weight4{{
    0u,4u,9u,13u,17u,21u,26u,30u,34u,38u,43u,47u,51u,55u,60u,64u
}};
void bc7_mode6_block(const std::uint8_t* block,std::uint8_t* rgba,
                     std::size_t pitch,std::size_t width,
                     std::size_t height) noexcept {
    std::array<std::array<unsigned,4>,2> endpoints{};
    const unsigned p0=bc7_bits(block,63u,1u);
    const unsigned p1=bc7_bits(block,64u,1u);
    // Channel-major fields: R0,R1,G0,G1,B0,B1,A0,A1 (7 bits each).
    for(unsigned channel=0;channel<4u;channel++){
        endpoints[0][channel]=(bc7_bits(block,7u+channel*14u,7u)<<1u)|p0;
        endpoints[1][channel]=(bc7_bits(block,14u+channel*14u,7u)<<1u)|p1;
    }
    for(std::size_t y=0;y<height;y++){
        for(std::size_t x=0;x<width;x++){
            const auto texel=static_cast<unsigned>(y*4u+x);
            const unsigned index=texel==0u
                ?bc7_bits(block,65u,3u)
                :bc7_bits(block,68u+(texel-1u)*4u,4u);
            const unsigned weight=Bc7Weight4[index];
            auto* out=rgba+y*pitch+x*4u;
            for(unsigned channel=0;channel<4u;channel++)
                out[channel]=static_cast<std::uint8_t>(
                    ((64u-weight)*endpoints[0][channel]+
                     weight*endpoints[1][channel]+32u)>>6u);
        }
    }
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
    if(kind!=Format::Bc1 && kind!=Format::Bc3 &&
       kind!=Format::Bc4 && kind!=Format::Bc5 &&
       kind!=Format::Bc7Mode6)
        return Status::UnsupportedFormat;
    const std::size_t width=src.width,height=src.height;
    const std::size_t block_columns=(width+3u)/4u,block_rows=(height+3u)/4u;
    const std::size_t block_bytes=
        (kind==Format::Bc3 || kind==Format::Bc5 ||
         kind==Format::Bc7Mode6)?16u:8u;
    std::size_t required_src=0,required_dest=0;
    if(!required(block_rows,src.block_pitch,block_columns*block_bytes,
                 required_src) ||
       !required(height,dst.pitch,width*4u,required_dest))
        return Status::InvalidStride;
    if(src.block_bytes<required_src || dst.bytes<required_dest)
        return Status::BufferTooShort;
    // BC7 mode 6 is the only implemented BC7 encoding. Fail the entire
    // image before writing ANY pixel if even a later compressed block is
    // another mode (including reserved 0x00). No magenta placeholder data.
    if(kind==Format::Bc7Mode6){
        for(std::size_t row=0;row<block_rows;row++)
            for(std::size_t col=0;col<block_columns;col++){
                const auto* block=src.blocks+row*src.block_pitch+col*16u;
                if((block[0]&0x7fu)!=0x40u)
                    return Status::UnsupportedBlockMode;
            }
    }
    // Full prevalidation before any output byte changes, including row padding.
    // The caller guarantees that compressed source and output buffers do not
    // overlap and remain owned for the entire synchronous conversion call.
    for(std::size_t by=0;by<block_rows;by++){
        for(std::size_t bx=0;bx<block_columns;bx++){
            const auto* block=src.blocks+by*src.block_pitch+bx*block_bytes;
            if(kind==Format::Bc7Mode6){
                const auto visible_w=(bx*4u+4u<=width)?4u:width-bx*4u;
                const auto visible_h=(by*4u+4u<=height)?4u:height-by*4u;
                bc7_mode6_block(block,dst.rgba+by*4u*dst.pitch+bx*16u,
                                dst.pitch,visible_w,visible_h);
                continue;
            }
            if(kind==Format::Bc4 || kind==Format::Bc5){
                // BC4: one 8-byte UNORM block -> red. BC5: two independent
                // 8-byte UNORM blocks -> red and green. B=0, A=255.
                const auto reds=alpha_table(block);
                const auto red_indices=index_bits48(block);
                std::array<std::uint8_t,8> greens{};
                std::uint64_t green_indices=0;
                if(kind==Format::Bc5){
                    greens=alpha_table(block+8u);
                    green_indices=index_bits48(block+8u);
                }
                for(std::size_t py=0;py<4;py++){
                    const auto y=by*4u+py;
                    if(y>=height)break;
                    for(std::size_t px=0;px<4;px++){
                        const auto x=bx*4u+px;
                        if(x>=width)break;
                        const auto index=static_cast<unsigned>(py*4u+px);
                        auto* out=dst.rgba+y*dst.pitch+x*4u;
                        out[0]=reds[(red_indices>>(3u*index))&7u];
                        out[1]=kind==Format::Bc5
                            ?greens[(green_indices>>(3u*index))&7u]:0u;
                        out[2]=0u;out[3]=255u;
                    }
                }
                continue;
            }
            const auto* colors=block+(kind==Format::Bc3?8u:0u);
            const auto table=color_table(colors,kind==Format::Bc3);
            const auto indices=u32(colors+4);
            std::array<std::uint8_t,8> alpha{};
            std::uint64_t alpha_bits=0;
            if(kind==Format::Bc3){
                alpha=alpha_table(block);
                alpha_bits=index_bits48(block);
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
        case Status::UnsupportedBlockMode:return "unsupported-block-mode";
    }
    return "unknown";
}
} // namespace scarlett::texture
