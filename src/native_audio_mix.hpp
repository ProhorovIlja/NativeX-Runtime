// Original bounded native PC stereo PCM reference mixer.
// Independent of proprietary game formats, middleware, device SDKs and sound output.
#pragma once
#include <cstddef>
#include <cstdint>
namespace scarlett::audio {
constexpr std::size_t MaxFrames=4096;
constexpr std::size_t MaxVoices=64;
struct Voice {
    const float* stereo_interleaved=nullptr; // separate, immutable input storage
    std::size_t frames=0;
    float gain=1.0f;
};
struct MixStats {
    std::uint64_t clipped_samples=0;
    std::uint64_t non_finite_samples=0;
    float peak=0.0f;
};
// Bounded single-threaded stereo float mixer, sample range [-1,1].
// Validates every input before touching output or stats. No allocation.
// Output must not overlap any input buffer; controls/text/UI unrelated.
bool mix_stereo(float* output,std::size_t frames,
                const Voice* voices,std::size_t voice_count,MixStats& stats) noexcept;
}
