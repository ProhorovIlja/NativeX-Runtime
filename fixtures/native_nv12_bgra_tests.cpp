// Original deterministic native NV12 surface conversion acceptance.
// Synthetic planes only; no video bitstream or hardware codec is decoded.
#include "native_nv12_bgra.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace scarlett::video;
namespace {
void check(bool ok,const char* reason) {
    if(!ok)throw std::runtime_error(reason);
}
void exact_studio_rgb(){
    std::array<std::uint8_t,8> luma{81,81,145,145,81,81,145,145};
    std::array<std::uint8_t,4> chroma{90,240,54,34};
    std::array<std::uint8_t,32> bgra{};
    Nv12 input{luma.data(),chroma.data(),luma.size(),chroma.size(),4,4,4,2};
    Bgra output{bgra.data(),bgra.size(),16};
    check(convert(input,output,Matrix::Bt601Limited)==Status::Ok,
          "4x2 exact studio-range conversion failed");
    for(int row=0;row<2;row++){
        for(int x=0;x<4;x++){
            const auto* pixel=bgra.data()+row*16+x*4;
            const bool red=x<2;
            check(pixel[0]==(red?0:1) && pixel[1]==(red?0:255) &&
                  pixel[2]==(red?255:0) && pixel[3]==255,
                  "BT.601 red/green expected BGRA values incorrect");
        }
    }
}
void odd_dimensions_and_padded_rows(){
    std::array<std::uint8_t,24> y{};
    std::array<std::uint8_t,16> uv{};
    std::array<std::uint8_t,84> out{};
    y.fill(16);uv.fill(128);out.fill(0xCC);
    y[8+2]=235; // white center; 5x3 image with 8-byte pitch.
    Nv12 input{y.data(),uv.data(),y.size(),uv.size(),8,8,5,3};
    Bgra target{out.data(),out.size(),28};
    check(convert(input,target,Matrix::Bt709Limited)==Status::Ok,
          "odd NV12 image with padded strides rejected");
    for(std::size_t row=0;row<3;row++){
        for(std::size_t x=0;x<5;x++){
            const auto* p=out.data()+row*28+x*4;
            const auto expected=(row==1&&x==2)?255:0;
            check(p[0]==expected && p[1]==expected && p[2]==expected &&
                  p[3]==255,"odd NV12 pixels were misaddressed");
        }
        for(std::size_t x=20;x<28;x++)
            check(out[row*28+x]==0xCC,
                  "NV12 output converter overwrote row padding");
    }
    check(uv[6]==128 && uv[14]==128,"NV12 input plane modified");
}
void one_by_one_and_color_matrix_difference(){
    const std::array<std::uint8_t,1> luma{128};
    const std::array<std::uint8_t,2> uv{70,180};
    std::array<std::uint8_t,4> a{},b{};
    const Nv12 input{luma.data(),uv.data(),luma.size(),uv.size(),1,2,1,1};
    check(convert(input,{a.data(),a.size(),4},Matrix::Bt601Limited)==Status::Ok,
          "smallest odd NV12 image failed");
    check(convert(input,{b.data(),b.size(),4},Matrix::Bt709Limited)==Status::Ok,
          "smallest odd BT709 image failed");
    check(a!=b && a[3]==255 && b[3]==255,
          "BT.601 vs BT.709 produced identical chromatic pixel");
}
void reject_invalid_without_mutating_output(){
    std::array<std::uint8_t,24> y{};
    std::array<std::uint8_t,16> uv{};
    std::array<std::uint8_t,84> out{};
    y.fill(16);uv.fill(128);out.fill(0xA7);
    const Nv12 valid{y.data(),uv.data(),y.size(),uv.size(),8,8,5,3};
    const Bgra destination{out.data(),out.size(),28};
    auto expect=[&](Nv12 input,Bgra target,Matrix matrix,Status wanted){
        check(convert(input,target,matrix)==wanted,"invalid NV12 geometry accepted");
        for(const auto byte:out)check(byte==0xA7,"invalid conversion wrote output");
    };
    auto changed=valid;
    changed.width=0;
    expect(changed,destination,Matrix::Bt601Limited,Status::InvalidDimensions);
    changed=valid;changed.height=8193;
    expect(changed,destination,Matrix::Bt601Limited,Status::InvalidDimensions);
    changed=valid;changed.y=nullptr;
    expect(changed,destination,Matrix::Bt601Limited,Status::NullBuffer);
    changed=valid;changed.uv=nullptr;
    expect(changed,destination,Matrix::Bt601Limited,Status::NullBuffer);
    expect(valid,{nullptr,destination.bytes,destination.pitch},
           Matrix::Bt601Limited,Status::NullBuffer);
    changed=valid;changed.y_pitch=4;
    expect(changed,destination,Matrix::Bt601Limited,Status::InvalidStride);
    changed=valid;changed.uv_pitch=5;
    expect(changed,destination,Matrix::Bt601Limited,Status::InvalidStride);
    expect(valid,{out.data(),out.size(),19},
           Matrix::Bt601Limited,Status::InvalidStride);
    changed=valid;changed.y_bytes=20;
    expect(changed,destination,Matrix::Bt601Limited,Status::BufferTooShort);
    changed=valid;changed.uv_bytes=13;
    expect(changed,destination,Matrix::Bt601Limited,Status::BufferTooShort);
    expect(valid,{out.data(),75,28},
           Matrix::Bt601Limited,Status::BufferTooShort);
    expect(valid,destination,static_cast<Matrix>(255),Status::UnknownMatrix);
    changed=valid;changed.y_pitch=std::numeric_limits<std::size_t>::max();
    expect(changed,destination,Matrix::Bt709Limited,Status::InvalidStride);
    check(std::string(status_name(Status::BufferTooShort))=="buffer-too-short",
          "error classification label missing");
}
}
int main(){
    try{
        exact_studio_rgb();
        odd_dimensions_and_padded_rows();
        one_by_one_and_color_matrix_difference();
        reject_invalid_without_mutating_output();
    }catch(const std::exception& e){
        std::cerr<<"native NV12 surface test FAILED: "<<e.what()<<"\n";
        return 1;
    }
    std::cout<<"{\"kind\":\"native-windows-linux-nv12-to-bgra8\","
             "\"scenarios\":4,"
             "\"odd_dimensions_and_strides_verified\":true,"
             "\"bt601_bt709_limited_supported\":true,"
             "\"red_green_exact_pixel_oracle\":true,"
             "\"invalid_input_no_write\":true,"
             "\"hardware_video_decoder_tested\":false,"
             "\"title_cutscene_tested\":false}\n";
    return 0;
}
