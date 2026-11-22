// Portable stereo float PCM sample-rate conversion reference (not a codec).
#pragma once
#include <cstddef>
#include <cstdint>
namespace scarlett::audio {
constexpr std::size_t ResampleMaxOutputFrames=4096;
constexpr std::size_t ResampleMaxInputFrames=65536;
enum class ResampleStatus { Ok, Invalid, NeedMoreInput };
// Absolute input sample position numerator, with output_hz as denominator.
// Reset to {} when the source or target sample rate changes.
struct ResampleCursor {
    std::uint64_t phase=0;
    std::uint32_t source_hz=0;
    std::uint32_t output_hz=0;
};
// Does not allocate. Validates the entire requested range before writing.
// input is stereo-interleaved frames [first_source_frame, first_source_frame+input_frames).
// output must not overlap input; each call continues the same absolute cursor.
// Invalid / insufficient input must leave output and cursor unchanged.
ResampleStatus resample_stereo_linear(
    const float* input, std::size_t input_frames,
    std::uint64_t first_source_frame, float* output, std::size_t output_frames,
    std::uint32_t source_hz, std::uint32_t output_hz,
    ResampleCursor& cursor) noexcept;
} // namespace scarlett::audio
