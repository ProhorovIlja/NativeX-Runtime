// Original synthetic BC1 / BC3 compressed block CPU oracle and malformed data.
// 4x4 block raster is linear D3D layout, not proprietary console swizzle.
#include "native_bc_texture.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
using namespace scarlett::texture;
namespace {
void check(bool cond,const char* why){if(!cond)throw std::runtime_error(why);}
void le16(std::uint8_t* p,std::uint16_t x){
    p[0]=static_cast<std::uint8_t>(x);
    p[1]=static_cast<std::uint8_t>(x>>8u);
}
void le32(std::uint8_t* p,std::uint32_t x){
    for(unsigned i=0;i<4;i++)p[i]=static_cast<std::uint8_t>(x>>(i*8u));
}
void solid(std::uint8_t* block,std::uint16_t rgb565){
    le16(block,rgb565);le16(block+2,0);le32(block+4,0);
}
void rgba(const std::uint8_t* p,unsigned r,unsigned g,unsigned b,unsigned a){
    check(p[0]==r && p[1]==g && p[2]==b && p[3]==a,
          "compressed texture pixel mismatched CPU reference");
}
void four_color_bc1_palette(){
    std::array<std::uint8_t,8> block{};
    le16(block.data(),0xF800u); // RGB565 pure red
    le16(block.data()+2,0x001Fu); // blue; c0>c1
    le32(block.data()+4,0x000000E4u); // pixel codes 0,1,2,3
    std::array<std::uint8_t,64> result{};
    check(decode({block.data(),block.size(),8,4,4},
                 {result.data(),result.size(),16},Format::Bc1)==Status::Ok,
          "BC1 opaque block decode failure");
    rgba(result.data()+0,255,0,0,255);
    rgba(result.data()+4,0,0,255,255);
    rgba(result.data()+8,170,0,85,255);
    rgba(result.data()+12,85,0,170,255);
}
void bc1_transparent_and_midpoint(){
    std::array<std::uint8_t,8> block{};
    le16(block.data(),0x0000u);
    le16(block.data()+2,0xFFFFu);
    le32(block.data()+4,0x000000E4u); // 0,1,2,3
    std::array<std::uint8_t,64> result{};
    check(decode({block.data(),block.size(),8,4,4},
                 {result.data(),result.size(),16},Format::Bc1)==Status::Ok,
          "BC1 1-bit-alpha block decode failure");
    rgba(result.data()+0,0,0,0,255);
    rgba(result.data()+4,255,255,255,255);
    rgba(result.data()+8,127,127,127,255);
    rgba(result.data()+12,0,0,0,0);
}
void bc3_independent_alpha_and_opaque_four_color(){
    std::array<std::uint8_t,16> block{};
    block[0]=255;block[1]=0;
    // Alpha codes for four pixels: 0,1,2,7. DXT5 little-endian 3-bit.
    const std::uint64_t alpha_index=0ull|(1ull<<3u)|(2ull<<6u)|(7ull<<9u);
    for(unsigned j=0;j<6;j++)
        block[j+2]=static_cast<std::uint8_t>(alpha_index>>(8u*j));
    // BC3 keeps 4-color interpolation even when c0<c1.
    le16(block.data()+8,0x0000u);le16(block.data()+10,0xffffu);
    le32(block.data()+12,0x000000E4u);
    std::array<std::uint8_t,64> out{};
    check(decode({block.data(),block.size(),16,4,4},
                 {out.data(),out.size(),16},Format::Bc3)==Status::Ok,
          "BC3 alpha block decode failure");
    rgba(out.data()+0,0,0,0,255);
    rgba(out.data()+4,255,255,255,0);
    rgba(out.data()+8,85,85,85,218);
    rgba(out.data()+12,170,170,170,36);
    block[0]=0;block[1]=255;
    check(decode({block.data(),block.size(),16,4,4},
                 {out.data(),out.size(),16},Format::Bc3)==Status::Ok,
          "BC3 6-level alpha mode rejected");
    rgba(out.data()+0,0,0,0,0);
    rgba(out.data()+4,255,255,255,255);
    rgba(out.data()+8,85,85,85,51);
    rgba(out.data()+12,170,170,170,255);
}
void odd_block_dimensions_and_padded_raster(){
    // The exact 5x7 pixel raster needs 2 columns x 2 rows of 4x4 blocks.
    // Power-of-two-only column masks would incorrectly read the last column.
    constexpr std::size_t compressed_pitch=21,output_pitch=24;
    std::array<std::uint8_t,compressed_pitch*2> blocks{};
    std::array<std::uint8_t,output_pitch*7> out{};
    blocks.fill(0xD7);out.fill(0xB2);
    solid(blocks.data()+0,0xF800u);  // red, top-left
    solid(blocks.data()+8,0x07E0u);  // green, top-right
    solid(blocks.data()+compressed_pitch,0x001Fu); // blue, bottom-left
    solid(blocks.data()+compressed_pitch+8,0xFFFFu); // white, bottom-right
    check(decode({blocks.data(),blocks.size(),compressed_pitch,5,7},
                 {out.data(),out.size(),output_pitch},Format::Bc1)==Status::Ok,
          "odd-width padded BC1 5x7 raster decode failure");
    for(std::size_t y=0;y<7;y++){
        for(std::size_t x=0;x<5;x++){
            auto* pixel=out.data()+y*output_pitch+x*4u;
            const bool top=y<4,left=x<4;
            rgba(pixel,
                 (top&&left)||(!top&&!left)?255:0,
                 (top&&!left)||(!top&&!left)?255:0,
                 (!top&&left)||(!top&&!left)?255:0,255);
        }
        for(std::size_t p=20;p<output_pitch;p++)
            check(out[y*output_pitch+p]==0xB2,"output row padding overwritten");
    }
    for(std::size_t row=0;row<2;row++)
        for(std::size_t i=16;i<compressed_pitch;i++)
            check(blocks[row*compressed_pitch+i]==0xD7,
                  "compressed block row padding modified");
}
void malformed_inputs_never_write(){
    std::array<std::uint8_t,32> source{};
    std::array<std::uint8_t,64> output{};
    output.fill(0xC4);
    const Surface good{source.data(),source.size(),16,5,3};
    const Output dest{output.data(),output.size(),20};
    auto expect=[&](Surface src,Output dst,Format kind,Status error){
        check(decode(src,dst,kind)==error,"invalid block-surface input accepted");
        for(auto byte:output)
            check(byte==0xC4,"rejected BCn image modified caller output");
    };
    expect({nullptr,source.size(),16,5,3},dest,Format::Bc1,Status::NullPointer);
    expect(good,{nullptr,output.size(),20},Format::Bc1,Status::NullPointer);
    auto bad=good;bad.width=0;
    expect(bad,dest,Format::Bc1,Status::InvalidDimensions);
    bad=good;bad.width=8193;
    expect(bad,dest,Format::Bc1,Status::InvalidDimensions);
    bad=good;bad.block_pitch=15;
    expect(bad,dest,Format::Bc1,Status::InvalidStride);
    expect(good,{output.data(),output.size(),19},Format::Bc1,Status::InvalidStride);
    bad=good;bad.block_bytes=15;
    expect(bad,dest,Format::Bc1,Status::BufferTooShort);
    expect(good,{output.data(),59,20},Format::Bc1,Status::BufferTooShort);
    expect(good,dest,static_cast<Format>(99),Status::UnsupportedFormat);
    bad=good;bad.block_pitch=std::numeric_limits<std::size_t>::max();
    expect(bad,dest,Format::Bc1,Status::InvalidStride);
    bad=good;bad.block_pitch=32;bad.block_bytes=31;
    expect(bad,dest,Format::Bc3,Status::BufferTooShort);
}
}
int main(){
    try{
        four_color_bc1_palette();
        bc1_transparent_and_midpoint();
        bc3_independent_alpha_and_opaque_four_color();
        odd_block_dimensions_and_padded_raster();
        malformed_inputs_never_write();
    }catch(const std::exception& e){
        std::cerr<<"native BC1/BC3 decoder FAIL: "<<e.what()<<"\n";
        return 1;
    }
    std::cout<<"{\"kind\":\"native-bc1-bc3-rectangular-texel-raster\","
             "\"scenarios\":5,"
             "\"rgb565_4color_and_cutout_verified\":true,"
             "\"bc3_alpha_8_and_6_mode_verified\":true,"
             "\"non_power_of_two_raster_verified\":true,"
             "\"invalid_input_prevalidated\":true,"
             "\"bc7_decoded\":false,"
             "\"console_swizzle_resolved\":false,"
             "\"game_textures_used\":false}\n";
    return 0;
}
