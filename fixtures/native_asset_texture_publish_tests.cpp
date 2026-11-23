// Original bounded native Reader -> CPU BC texture all-or-nothing publication.
#include "native_asset_texture_publish.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
using scarlett::stream::Priority;
using scarlett::stream::Reader;
using scarlett::stream::Request;
using scarlett::stream::Result;
using scarlett::texture::Format;
using scarlett::texturestream::Status;
using scarlett::texturestream::publish;
namespace {
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
struct Files {
    fs::path dir;
    Files(){
        const auto t=std::chrono::steady_clock::now().time_since_epoch().count();
        dir=fs::temp_directory_path()/("scarlett-bc-stream-test-"+std::to_string(t));
        if(!fs::create_directory(dir))throw std::runtime_error("fixture temp collision");
    }
    ~Files(){std::error_code e;fs::remove_all(dir,e);}
    void write(const fs::path& file,const std::vector<std::uint8_t>& bytes){
        std::ofstream out(file,std::ios::binary|std::ios::trunc);
        check(out.is_open(),"unable to create synthetic BC stream");
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        check(out.good(),"unable to write fixture bytes");
    }
    std::vector<std::uint8_t> read(const fs::path& file){
        std::ifstream in(file,std::ios::binary);
        return {std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    }
};
std::array<std::uint8_t,8> bc1_solid(std::uint16_t color) {
    // A minimal 4x4 RGB565 block; all texels index 0.
    return {{static_cast<std::uint8_t>(color),
             static_cast<std::uint8_t>(color>>8u),0,0,0,0,0,0}};
}
void rgba_pixel(const std::vector<std::uint8_t>& pixels,std::size_t offset,
                std::uint8_t red,std::uint8_t green,std::uint8_t blue){
    check(offset+3<pixels.size() &&
          pixels[offset]==red && pixels[offset+1]==green &&
          pixels[offset+2]==blue && pixels[offset+3]==255,
          "published RGBA8 differs from independently expected solid palette");
}
void completed_archive_records_publish_in_order(){
    Files files;
    const auto file=files.dir/"two_complete_bc1_records.bin";
    const auto red=bc1_solid(0xf800),green=bc1_solid(0x07e0);
    std::vector<std::uint8_t> bytes{};
    bytes.insert(bytes.end(),red.begin(),red.end());
    bytes.insert(bytes.end(),green.begin(),green.end());
    files.write(file,bytes);
    Reader reader(2,16);
    const auto first=reader.submit(Request{file,0,8,{},Priority::FrameCritical}).get();
    const auto second=reader.submit(Request{file,8,8,{},Priority::Normal}).get();
    check(first.status==scarlett::stream::Status::Ok &&
          second.status==scarlett::stream::Status::Ok,
          "native Reader failed to fetch completed records");
    const auto texture1=publish(first,Format::Bc1,4,4,8);
    const auto texture2=publish(second,Format::Bc1,4,4,8);
    check(texture1.status==Status::Ready && texture2.status==Status::Ready &&
          texture1.rgba.size()==64 && texture2.rgba.size()==64,
          "complete records not both published");
    for(std::size_t i=0;i<16;i++){
        rgba_pixel(texture1.rgba,i*4,255,0,0);
        rgba_pixel(texture2.rgba,i*4,0,255,0);
    }
    check(files.read(file)==bytes,"publishing accidentally modified original data");
}
void unfinished_second_record_never_publishes(){
    Files files;
    const auto file=files.dir/"second_incomplete_bc1.bin";
    const auto red=bc1_solid(0xf800),green=bc1_solid(0x07e0);
    std::vector<std::uint8_t> bytes(red.begin(),red.end());
    bytes.insert(bytes.end(),green.begin(),green.begin()+4);
    files.write(file,bytes);
    Reader reader(1,8);
    const auto completed=reader.submit({file,0,8,{},Priority::FrameCritical}).get();
    const auto incomplete=reader.submit({file,8,8,{},Priority::Normal}).get();
    const auto first=publish(completed,Format::Bc1,4,4,8);
    const auto second=publish(incomplete,Format::Bc1,4,4,8);
    check(first.status==Status::Ready && first.rgba.size()==64,
          "complete first record was mistakenly withheld");
    check(incomplete.status==scarlett::stream::Status::InvalidRange &&
          second.status==Status::ReadFailed && second.rgba.empty(),
          "truncated second record was published as a texture");
}
void format_lengths_and_integrity_fail_closed(){
    const auto red=bc1_solid(0xf800);
    Result read{scarlett::stream::Status::Ok,
                std::vector<std::uint8_t>(red.begin(),red.end()),0};
    read.checksum=scarlett::stream::fnv1a64(read.bytes);
    auto inspect=[&](const Result& source,Format kind,
                     std::uint32_t width,std::uint32_t height,
                     std::size_t pitch,Status expected){
        const auto output=publish(source,kind,width,height,pitch);
        check(output.status==expected && output.rgba.empty() &&
              output.width==0 && output.height==0,
              "rejected source exposed a partially initialized RGBA texture");
    };
    auto wrong=read;wrong.bytes[0]^=1;
    inspect(wrong,Format::Bc1,4,4,8,Status::InputChecksumMismatch);
    inspect(read,Format::Bc1,0,4,8,Status::InvalidDimensions);
    inspect(read,Format::Bc1,4,8193,8,Status::InvalidDimensions);
    inspect(read,Format::Bc1,8192,8192,8192,Status::OutputBudgetExceeded);
    inspect(read,Format::Bc1,5,4,8,Status::InvalidStride);
    inspect(read,Format::Bc1,4,8,9,Status::CompressedByteCountMismatch);
    inspect(read,Format::Bc3,4,4,16,Status::CompressedByteCountMismatch);
    inspect(read,static_cast<Format>(200),4,4,8,Status::InvalidFormat);
    inspect(read,Format::Bc1,5,5,
            static_cast<std::size_t>(-1),Status::InvalidStride);
    auto cancelled=read;cancelled.status=scarlett::stream::Status::Cancelled;
    inspect(cancelled,Format::Bc1,4,4,8,Status::ReadFailed);
    check(std::string(scarlett::texturestream::name(Status::Ready))=="ready" &&
          std::string(scarlett::texturestream::name(Status::DecodeFailed))=="decode-failed",
          "published status naming changed");
}
void bc7_incomplete_mode_is_refused_atomically(){
    Result bad{scarlett::stream::Status::Ok,std::vector<std::uint8_t>(16,0),0};
    bad.checksum=scarlett::stream::fnv1a64(bad.bytes);
    const auto output=publish(bad,Format::Bc7Mode6,4,4,16);
    check(output.status==Status::DecodeFailed &&
          output.decode_status==scarlett::texture::Status::UnsupportedBlockMode &&
          output.rgba.empty(),"unsupported BC7 mode exposed damaged pixels");
    // A source-built valid BC7 mode6 sample from native_bc_texture fixture.
    bad.bytes={0xc0,0x3f,0x00,0xf0,0x07,0xfc,0xff,0x80,
               0xf0,0x78,0x10,0x32,0x54,0x76,0x98,0xba};
    bad.checksum=scarlett::stream::fnv1a64(bad.bytes);
    const auto good=publish(bad,Format::Bc7Mode6,4,4,16);
    check(good.status==Status::Ready && good.rgba.size()==64,
          "valid source-built BC7 mode6 failed to publish");
    const auto& pixel=good.rgba;
    check(pixel[0]==255 && pixel[1]==1 && pixel[2]==1 && pixel[3]==255,
          "BC7 mode6 decoded output mismatch");
}
void rectangular_compressed_pitch_and_visible_texels(){
    Files files;
    const auto file=files.dir/"padded_5x7_bc1.bin";
    const auto red=bc1_solid(0xf800),green=bc1_solid(0x07e0);
    const auto blue=bc1_solid(0x001f),white=bc1_solid(0xffff);
    std::vector<std::uint8_t> pixels{};
    pixels.insert(pixels.end(),red.begin(),red.end());
    pixels.insert(pixels.end(),green.begin(),green.end());
    pixels.push_back(0xae);pixels.push_back(0xaf); // compressed row padding
    pixels.insert(pixels.end(),blue.begin(),blue.end());
    pixels.insert(pixels.end(),white.begin(),white.end());
    files.write(file,pixels);
    Reader reader(1,8);
    const auto read=reader.submit({file,0,pixels.size(),{},Priority::Background}).get();
    const auto out=publish(read,Format::Bc1,5,7,18);
    check(out.status==Status::Ready && out.rgba.size()==140,
          "odd rectangular padded compressed layout failed");
    for(std::size_t y=0;y<7;y++){
        for(std::size_t x=0;x<5;x++){
            const auto offset=(y*5+x)*4;
            if(y<4 && x<4)rgba_pixel(out.rgba,offset,255,0,0);
            else if(y<4)rgba_pixel(out.rgba,offset,0,255,0);
            else if(x<4)rgba_pixel(out.rgba,offset,0,0,255);
            else rgba_pixel(out.rgba,offset,255,255,255);
        }
    }
}
}
int main(){
    try{
        completed_archive_records_publish_in_order();
        unfinished_second_record_never_publishes();
        format_lengths_and_integrity_fail_closed();
        bc7_incomplete_mode_is_refused_atomically();
        rectangular_compressed_pitch_and_visible_texels();
    }catch(const std::exception& e){
        std::cerr<<"native BC texture stream publish FAIL: "<<e.what()<<'\n';
        return 1;
    }
    std::cout<<"{\"kind\":\"native-bc-stream-publication\","
             "\"scenario_groups\":5,\"real_os_file_io\":true,"
             "\"synthetic_bc_records\":true,\"publisher_atomic\":true,"
             "\"gta_vi_tested\":false,\"gpu_residency_tested\":false}\n";
    return 0;
}
