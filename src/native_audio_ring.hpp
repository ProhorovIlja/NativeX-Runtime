#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace scarlett::audio {
// Single producer, single consumer, stereo interleaved float PCM. No allocation,
// locks or waits after construction. Never reset while either owner is active.
// Input/output buffers are externally owned, valid and non-overlapping.
// Producer supplies finite PCM samples; this transport does not process audio.
class PcmRing {
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                  "requires lock-free 32-bit atomics");
    static constexpr std::uint32_t MaxFrames=65536;
    static std::uint32_t checked(std::uint32_t n) {
        if(!n || n>MaxFrames || (n&(n-1u)))throw std::invalid_argument("PCM ring capacity");
        return n;
    }
    const std::uint32_t capacity_,mask_;
    std::vector<float> samples_;
    alignas(64) std::atomic<std::uint32_t> write_;
    alignas(64) std::atomic<std::uint32_t> read_;
public:
    // Initial sequence may be arbitrary; empty at construction. Unsigned
    // subtraction remains valid across wrap because occupancy <= capacity.
    explicit PcmRing(std::uint32_t frames,std::uint32_t initial_sequence=0)
        :capacity_(checked(frames)),mask_(capacity_-1u),
         samples_(static_cast<std::size_t>(capacity_)*2u),
         write_(initial_sequence),read_(initial_sequence) {}
    PcmRing(const PcmRing&)=delete;
    PcmRing& operator=(const PcmRing&)=delete;
    std::uint32_t capacity() const noexcept {return capacity_;}
    // Producer only: all-or-nothing admission, no overwrite of unread audio.
    bool push(const float* input,std::uint32_t frames) noexcept {
        if(frames>capacity_ || (frames&&!input))return false;
        if(!frames)return true;
        const auto w=write_.load(std::memory_order_relaxed);
        const auto r=read_.load(std::memory_order_acquire);
        if(frames>capacity_-(w-r))return false;
        const auto pos=w&mask_;
        const auto first=std::min(frames,capacity_-pos);
        std::memcpy(samples_.data()+2u*pos,input,2u*first*sizeof(float));
        if(first<frames)std::memcpy(samples_.data(),input+2u*first,2u*(frames-first)*sizeof(float));
        write_.store(w+frames,std::memory_order_release);
        return true;
    }
    // Consumer only: invalid requests leave output and indices unchanged.
    // Valid requests always initialize the whole output; underruns use silence.
    // Return value separates consumed audio from zero-filled output.
    struct Read {bool valid;std::uint32_t delivered,silence;};
    Read pull(float* output,std::uint32_t frames) noexcept {
        if(frames>capacity_ || (frames&&!output))return {false,0,0};
        if(!frames)return {true,0,0};
        const auto r=read_.load(std::memory_order_relaxed);
        const auto w=write_.load(std::memory_order_acquire);
        const auto count=std::min(frames,w-r);
        const auto pos=r&mask_;
        const auto first=std::min(count,capacity_-pos);
        if(first)std::memcpy(output,samples_.data()+2u*pos,2u*first*sizeof(float));
        if(first<count)std::memcpy(output+2u*first,samples_.data(),2u*(count-first)*sizeof(float));
        std::fill(output+2u*count,output+2u*frames,0.0f);
        read_.store(r+count,std::memory_order_release);
        return {true,count,frames-count};
    }
};
}
