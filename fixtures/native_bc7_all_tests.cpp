// Deterministic native CPU BC7-all-modes source-built regression tests.
// This is not Xbox tiling, BCPack or a GTA VI hardware texture test.
#include "native_bc_texture.hpp"
#include "native_asset_texture_publish.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using scarlett::texture::Format;
using scarlett::texture::Status;
using scarlett::texture::decode;
namespace {
void require(bool condition,const char* what){
    if(!condition)throw std::runtime_error(what);
}
std::array<std::uint8_t,16> make_block(unsigned mode,unsigned seed){
    std::array<std::uint8_t,16> block{};
    // First set bit identifies the BC7 encoding mode 0..7.
    block[0]=static_cast<std::uint8_t>(1u<<mode);
    for(std::size_t i=1;i<block.size();i++)
        block[i]=static_cast<std::uint8_t>((i*37u+seed*13u)&255u);
    return block;
}
void modes_are_recognized_and_reproducible(){
    for(unsigned mode=0;mode<8;mode++){
        const auto block=make_block(mode,9);
        std::array<std::uint8_t,64> a{},b{};
        a.fill(0xa5);b.fill(0x5a);
        const scarlett::texture::Surface src{block.data(),block.size(),16,4,4};
        require(decode(src,{a.data(),a.size(),16},Format::Bc7All)==Status::Ok,
                "legal BC7 unary mode was rejected");
        require(decode(src,{b.data(),b.size(),16},Format::Bc7All)==Status::Ok &&
                a==b,"full BC7 decoder was not deterministic");
        scarlett::stream::Result result{};
        result.status=scarlett::stream::Status::Ok;
        result.bytes.assign(block.begin(),block.end());
        result.checksum=scarlett::stream::fnv1a64(result.bytes);
        const auto published=scarlett::texturestream::publish(
            result,Format::Bc7All,4,4,16);
        require(published.status==scarlett::texturestream::Status::Ready &&
                published.rgba==std::vector<std::uint8_t>(a.begin(),a.end()),
                "Reader BC7-all-format publication does not match direct decode");
    }
}
void mode6_reference_agrees_with_original(){
    // Existing independently generated endpoint/anchor sample from earlier
    // native BC7 mode-6 GPU/CPU differential, not a game asset.
    const std::array<std::uint8_t,16> sample{{
        0xc0,0x3f,0x00,0xf0,0x07,0xfc,0xff,0x80,
        0xf0,0x78,0x10,0x32,0x54,0x76,0x98,0xba
    }};
    std::array<std::uint8_t,64> original{},reference{};
    const scarlett::texture::Surface src{sample.data(),sample.size(),16,4,4};
    require(decode(src,{original.data(),64,16},Format::Bc7Mode6)==Status::Ok &&
            decode(src,{reference.data(),64,16},Format::Bc7All)==Status::Ok,
            "mode6 dual decoders rejected known valid reference");
    for(std::size_t i=0;i<64;i++){
        const auto diff=std::abs(static_cast<int>(original[i])-
                                 static_cast<int>(reference[i]));
        require(diff<=1,"bcdec mode6 differs from existing CPU mode6 oracle");
    }
}
void rectangular_rows_padding_and_atomic_invalid_mode(){
    constexpr std::size_t src_pitch=39,dst_pitch=27;
    std::array<std::uint8_t,src_pitch*2u> src{};
    std::array<std::uint8_t,dst_pitch*7u> out{};
    src.fill(0xce);out.fill(0xa9);
    const std::array<unsigned,4> modes{{0,1,6,7}};
    for(std::size_t by=0;by<2;by++)
        for(std::size_t bx=0;bx<2;bx++){
            const auto b=make_block(modes[by*2+bx],11);
            std::copy(b.begin(),b.end(),
                      src.begin()+static_cast<std::ptrdiff_t>(
                          by*src_pitch+bx*16u));
        }
    const scarlett::texture::Surface source{src.data(),src.size(),src_pitch,5,7};
    const scarlett::texture::Output dest{out.data(),out.size(),dst_pitch};
    require(decode(source,dest,Format::Bc7All)==Status::Ok,
            "nonaligned 5x7 BC7 layout failed");
    for(std::size_t y=0;y<7;y++)
        for(std::size_t off=20;off<dst_pitch;off++)
            require(out[y*dst_pitch+off]==0xa9,
                    "BC7 row padding overwritten");
    // Last block (last row, last column) has no legal mode prefix.
    src[src_pitch+16u]=0;
    out.fill(0xa9);
    require(decode(source,dest,Format::Bc7All)==Status::UnsupportedBlockMode,
            "invalid BC7 mode was accepted");
    require(std::all_of(out.begin(),out.end(),
                        [](std::uint8_t b){return b==0xa9;}),
            "invalid late BC7 block caused partial publication");
    require(decode({src.data(),15,16,4,4},
                   {out.data(),out.size(),dst_pitch},
                   Format::Bc7All)==Status::BufferTooShort,
            "truncated BC7 read unexpectedly accepted");
}
}
int main(){
    try{
        modes_are_recognized_and_reproducible();
        mode6_reference_agrees_with_original();
        rectangular_rows_padding_and_atomic_invalid_mode();
    }catch(const std::exception& ex){
        std::cerr<<"all-BC7 native reference FAIL: "<<ex.what()<<'\n';
        return 1;
    }
    std::cout<<"{\"bc7_modes\":8,\"source\":\"iOrange-bcdec-0.985\","
                 "\"rpf9_tested\":false,\"hardware_gpu_tested\":false,"
                 "\"gta_vi_tested\":false}\n";
    return 0;
}
