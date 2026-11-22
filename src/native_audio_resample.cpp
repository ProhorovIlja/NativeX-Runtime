#include "native_audio_resample.hpp"
#include <cmath>
#include <limits>
namespace scarlett::audio {
namespace {
float finite_or_silent(float value) noexcept {
    return std::isfinite(value) ? value : 0.0f;
}
bool legal_rate(std::uint32_t value) noexcept {
    return value>=8000u && value<=384000u;
}
}
ResampleStatus resample_stereo_linear(
    const float* input, std::size_t input_frames,
    std::uint64_t first_source_frame, float* output, std::size_t output_frames,
    std::uint32_t source_hz, std::uint32_t output_hz,
    ResampleCursor& cursor) noexcept {
    constexpr auto U64Max=std::numeric_limits<std::uint64_t>::max();
    if(!legal_rate(source_hz) || !legal_rate(output_hz) ||
       output_frames>ResampleMaxOutputFrames || input_frames>ResampleMaxInputFrames ||
       (output_frames && (!input || !output)) ||
       (cursor.source_hz && cursor.source_hz!=source_hz) ||
       (cursor.output_hz && cursor.output_hz!=output_hz) ||
       first_source_frame>U64Max-input_frames ||
       output_frames>(U64Max-cursor.phase)/source_hz)
        return ResampleStatus::Invalid;
    if(!output_frames) {
        cursor.source_hz=source_hz;
        cursor.output_hz=output_hz;
        return ResampleStatus::Ok;
    }
    const auto end_source_frame=first_source_frame+input_frames;
    const auto first_index=cursor.phase/output_hz;
    const auto last_phase=cursor.phase+(output_frames-1u)*static_cast<std::uint64_t>(source_hz);
    const auto last_index=last_phase/output_hz;
    // For exact integer phase the last sample is sufficient; for a fractional
    // position, interpolation needs the next complete input stereo frame.
    if(input_frames==0 || first_index<first_source_frame ||
       last_index>=end_source_frame ||
       (last_phase%output_hz!=0 && last_index+1>=end_source_frame))
        return ResampleStatus::NeedMoreInput;
    auto phase=cursor.phase;
    for(std::size_t out_frame=0;out_frame<output_frames;out_frame++) {
        const auto index=static_cast<std::size_t>(phase/output_hz-first_source_frame);
        const auto remainder=phase%output_hz;
        const double fraction=static_cast<double>(remainder)/output_hz;
        for(std::size_t channel=0;channel<2;channel++) {
            const auto a=static_cast<double>(finite_or_silent(input[index*2u+channel]));
            const auto b=remainder==0 ? a : static_cast<double>(
                finite_or_silent(input[(index+1u)*2u+channel]));
            output[out_frame*2u+channel]=static_cast<float>(a+(b-a)*fraction);
        }
        phase+=source_hz;
    }
    cursor.phase=phase;
    cursor.source_hz=source_hz;
    cursor.output_hz=output_hz;
    return ResampleStatus::Ok;
}
} // namespace scarlett::audio
