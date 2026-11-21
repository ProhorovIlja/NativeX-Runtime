#include "native_audio_mix.hpp"
#include <cmath>
namespace scarlett::audio {
bool mix_stereo(float* output,std::size_t frames,
                const Voice* voices,std::size_t voice_count,MixStats& stats) noexcept {
    if(frames>MaxFrames||voice_count>MaxVoices||
       (frames && !output)||(voice_count && !voices)) return false;
    for(std::size_t i=0;i<voice_count;i++){
        if(voices[i].frames<frames || (frames && !voices[i].stereo_interleaved) ||
           !std::isfinite(voices[i].gain) ||
           voices[i].gain < -8.0f || voices[i].gain > 8.0f) return false;
    }
    MixStats result{};
    for(std::size_t frame=0;frame<frames;frame++){
        for(std::size_t channel=0;channel<2;channel++){
            double sum=0.0;
            const auto index=frame*2+channel;
            for(std::size_t voice=0;voice<voice_count;voice++){
                const float sample=voices[voice].stereo_interleaved[index];
                if(!std::isfinite(sample)){
                    ++result.non_finite_samples;
                    continue;
                }
                sum+=static_cast<double>(sample)*
                     static_cast<double>(voices[voice].gain);
            }
            if(sum>1.0){sum=1.0;++result.clipped_samples;}
            else if(sum< -1.0){sum=-1.0;++result.clipped_samples;}
            const float value=static_cast<float>(sum);
            output[index]=value;
            const float amplitude=std::fabs(value);
            if(amplitude>result.peak)result.peak=amplitude;
        }
    }
    stats=result;
    return true;
}
}
