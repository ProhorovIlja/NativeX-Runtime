#include "native_audio_mix.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace scarlett::audio;
namespace {
void check(bool v,const char* msg){if(!v)throw std::runtime_error(msg);}
bool close(float a,float b){return std::fabs(a-b)<0.00001f;}
void basic_stereo(){
    std::array<float,4> source{0.5f,-0.25f,-0.75f,0.125f};
    std::array<float,4> output{6,6,6,6};
    Voice v{source.data(),2,0.5f};MixStats s{};
    check(mix_stereo(output.data(),2,&v,1,s),"single stereo mix rejected");
    check(close(output[0],0.25f)&&close(output[1],-0.125f)&&
          close(output[2],-0.375f)&&close(output[3],0.0625f),"stereo samples");
    check(s.clipped_samples==0 && s.non_finite_samples==0 &&
          close(s.peak,0.375f),"stereo stats");
}
void mixing_and_clipping(){
    std::array<float,2> a{0.8f,-0.8f},b{0.7f,-0.7f};
    const std::array<Voice,2> voices{{{a.data(),1,1.0f},{b.data(),1,1.0f}}};
    std::array<float,2> out{0,0};MixStats stats{};
    check(mix_stereo(out.data(),1,voices.data(),voices.size(),stats),"two voices");
    check(out[0]==1.0f && out[1]==-1.0f && stats.clipped_samples==2 &&
          stats.peak==1.0f,"saturated samples and stats");
}
void finite_sanitation(){
    std::array<float,4> values{std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity(),
                               0.25f,-0.25f};
    std::array<float,4> output{9,9,9,9};
    Voice v{values.data(),2,1.0f};MixStats stats{};
    check(mix_stereo(output.data(),2,&v,1,stats),"nonfinite sample mix");
    check(output[0]==0 && output[1]==0 && close(output[2],0.25f) &&
          close(output[3],-0.25f)&&stats.non_finite_samples==2,
          "nonfinite sample handling");
}
void invalid_fail_closed(){
    std::array<float,2> data{0.5f,0.75f};
    std::array<float,2> out{8,8};
    MixStats s{};s.clipped_samples=52;
    Voice bad{data.data(),1,std::numeric_limits<float>::quiet_NaN()};
    check(!mix_stereo(out.data(),1,&bad,1,s),"NaN gain accepted");
    bad.gain=99.0f;
    check(!mix_stereo(out.data(),1,&bad,1,s),"wild gain accepted");
    bad.gain=1.0f;bad.frames=0;
    check(!mix_stereo(out.data(),1,&bad,1,s),"insufficient input frame count");
    check(!mix_stereo(out.data(),MaxFrames+1,&bad,1,s),"unbounded frames");
    check(!mix_stereo(out.data(),1,nullptr,1,s),"null voice table");
    check(!mix_stereo(nullptr,1,&bad,1,s),"null output");
    check(out[0]==8 && out[1]==8 && s.clipped_samples==52,"invalid mutated buffers");
}
void silence_and_limits(){
    std::array<float,8> out{};
    out.fill(7.0f);MixStats stats{};stats.non_finite_samples=17;
    check(mix_stereo(out.data(),4,nullptr,0,stats),"silence");
    for(const auto f:out)check(f==0.0f,"silence output");
    check(stats.non_finite_samples==0 && stats.peak==0.0f,"silence stats");
    std::array<float,MaxFrames*2> source{};
    std::array<float,MaxFrames*2> buffer{};
    source.fill(0.001f);
    std::array<Voice,MaxVoices> voices{};
    for(auto& v:voices)v={source.data(),MaxFrames,1.0f};
    check(mix_stereo(buffer.data(),MaxFrames,voices.data(),voices.size(),stats),
          "max supported frame/voice mix");
    check(close(buffer[0],0.064f) && close(buffer.back(),0.064f),
          "reference max voices output");
}
}
int main(){
    try{
        basic_stereo();mixing_and_clipping();finite_sanitation();
        invalid_fail_closed();silence_and_limits();
    }catch(const std::exception& ex){
        std::cerr<<"native PCM reference FAILED: "<<ex.what()<<"\n";return 1;
    }
    std::cout<<"{\"kind\":\"native-pc-stereo-pcm-reference\","
             "\"scenarios\":5,\"max_frames\":"<<MaxFrames<<","
             "\"max_voices\":"<<MaxVoices<<","
             "\"bounded\":true,\"no_dynamic_allocation\":true,"
             "\"device_rendering_verified\":false,"
             "\"spatial_audio_or_codec_verified\":false,"
             "\"game_assets_tested\":false}\n";
    return 0;
}
