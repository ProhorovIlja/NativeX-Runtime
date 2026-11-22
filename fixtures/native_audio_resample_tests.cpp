// Cross-platform real C++17 PCM rate conversion tests, no device or game data.
#include "native_audio_resample.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace scarlett::audio;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool near(float a,float b){return std::fabs(a-b)<0.00001f;}
void upsample_and_chunk_boundary(){
    const std::array<float,10> src{{0,0,1,-1,0,0,-1,1,0,0}};
    std::array<float,16> all{},split_output{};
    ResampleCursor one{},split{};
    check(resample_stereo_linear(src.data(),5,0,all.data(),8,48000,96000,one)==ResampleStatus::Ok,
          "upsampling segment failed");
    check(resample_stereo_linear(src.data(),3,0,split_output.data(),4,48000,96000,split)==ResampleStatus::Ok,
          "first source chunk failed");
    check(resample_stereo_linear(src.data()+4,3,2,split_output.data()+8,4,48000,96000,split)==ResampleStatus::Ok,
          "second source chunk failed");
    check(one.phase==split.phase && one.phase==384000u,"chunk phase drift");
    for(std::size_t i=0;i<all.size();i++)
        check(near(all[i],split_output[i]),"discontinuity at chunk boundary");
    check(near(all[0],0)&&near(all[2],0.5f)&&near(all[4],1)&&near(all[6],0.5f),
          "interpolation numeric oracle");
}
void downsample_without_drift(){
    const std::array<float,12> src{{0,0,1,1,2,2,3,3,4,4,5,5}};
    std::array<float,6> out{};ResampleCursor cursor{};
    check(resample_stereo_linear(src.data(),6,0,out.data(),3,96000,48000,cursor)==ResampleStatus::Ok,
          "downsample failed");
    check(out[0]==0&&out[2]==2&&out[4]==4&&cursor.phase==288000u,
          "downsample spacing or phase");
    std::array<float,1> unchanged{{77}};
    check(resample_stereo_linear(src.data(),6,0,unchanged.data(),0,96000,48000,cursor)==ResampleStatus::Ok &&
          unchanged[0]==77 && cursor.phase==288000u,"zero-length request mutated state");
}
void incomplete_source_is_atomic(){
    const std::array<float,4> src{{1,2,3,4}};
    std::array<float,8> out{{77,77,77,77,77,77,77,77}};
    ResampleCursor cursor{};
    check(resample_stereo_linear(src.data(),2,0,out.data(),4,48000,96000,cursor)==ResampleStatus::NeedMoreInput,
          "missing interpolation neighbor accepted");
    check(cursor.phase==0 && cursor.source_hz==0 && cursor.output_hz==0,
          "insufficient-data request mutated cursor");
    for(float v:out)check(v==77,"insufficient-data request changed output");
    cursor.phase=96000u;
    check(resample_stereo_linear(src.data(),2,0,out.data(),1,48000,96000,cursor)==ResampleStatus::Ok &&
          out[0]==3 && out[1]==4,"exact last sample required unneeded next frame");
}
void invalid_and_nonfinite(){
    const std::array<float,4> src{{std::numeric_limits<float>::quiet_NaN(),
                                    std::numeric_limits<float>::infinity(),1,1}};
    std::array<float,2> out{{99,99}};ResampleCursor cursor{};
    check(resample_stereo_linear(src.data(),2,0,out.data(),1,48000,48000,cursor)==ResampleStatus::Ok &&
          out[0]==0 && out[1]==0,"NaN/Inf leaked into audio output");
    const auto previous=cursor.phase;
    out={{99,99}};
    check(resample_stereo_linear(src.data(),2,0,out.data(),1,48000,96000,cursor)==ResampleStatus::Invalid,
          "rate change accepted without cursor reset");
    check(out[0]==99&&out[1]==99&&cursor.phase==previous,"bad rate mutated output");
    check(resample_stereo_linear(nullptr,0,0,out.data(),1,48000,48000,cursor)==ResampleStatus::Invalid,
          "null input accepted");
    check(resample_stereo_linear(src.data(),2,0,out.data(),1,1,48000,cursor)==ResampleStatus::Invalid,
          "invalid rate accepted");
    cursor.phase=UINT64_MAX;
    check(resample_stereo_linear(src.data(),2,0,out.data(),1,48000,48000,cursor)==ResampleStatus::Invalid,
          "phase overflow accepted");
}
}
int main(){
    try {
        upsample_and_chunk_boundary();
        downsample_without_drift();
        incomplete_source_is_atomic();
        invalid_and_nonfinite();
    }catch(const std::exception& ex){
        std::cerr<<"native audio resample FAILED: "<<ex.what()<<'\n';
        return 1;
    }
    std::cout<<"{\"kind\":\"native-pcm-rate-conversion\","
                "\"scenarios\":4,\"linear_only\":true,"
                "\"hardware_audio\":false,\"game_audio\":false}\n";
    return 0;
}
