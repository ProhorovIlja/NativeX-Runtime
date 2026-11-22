#include "native_frame_timeline.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <time.h>
#endif

namespace scarlett::timing {
namespace {
constexpr std::uint64_t Billion=1000000000ull;
constexpr std::uint64_t MaxFrameSpan=60ull*Billion;
using Values=std::array<std::uint64_t,Timeline::Capacity>;
std::size_t rank_index(std::size_t count,std::size_t numerator,
                       std::size_t denominator) noexcept {
    // ceil(count * numerator / denominator) - 1, count<=256.
    return (count*numerator+denominator-1u)/denominator-1u;
}
Metric describe(Values& values,std::size_t count) noexcept {
    Metric m{};m.count=count;
    if(!count)return m;
    std::sort(values.begin(),values.begin()+static_cast<std::ptrdiff_t>(count));
    m.minimum_ns=values[0];
    m.median_ns=values[rank_index(count,50,100)];
    m.p95_ns=values[rank_index(count,95,100)];
    m.p99_ns=values[rank_index(count,99,100)];
    m.maximum_ns=values[count-1];
    return m;
}
bool valid(const Phases& p) noexcept {
    if(!p.simulation_begin_ns ||
       p.simulation_begin_ns>p.simulation_end_ns ||
       p.simulation_end_ns>p.render_submit_ns ||
       p.render_submit_ns>p.present_return_ns ||
       (p.input_received_ns && p.input_received_ns>p.simulation_begin_ns))
        return false;
    return p.present_return_ns-p.simulation_begin_ns<=MaxFrameSpan &&
           (!p.input_received_ns ||
            p.present_return_ns-p.input_received_ns<=MaxFrameSpan);
}
}
bool Timeline::record(const Phases& sample) noexcept {
    if(!valid(sample)){++rejected_;return false;}
    if(count_==Capacity)++overwritten_;
    else ++count_;
    ring_[write_]=sample;
    write_=(write_+1u)%Capacity;
    return true;
}
Summary Timeline::summarize() const noexcept {
    Summary result{};result.overwritten=overwritten_;result.rejected=rejected_;
    Values simulation{}, submit{}, cpu{}, input{};
    std::size_t input_count=0;
    for(std::size_t i=0;i<count_;i++){
        const std::size_t oldest=(write_+Capacity-count_)%Capacity;
        const auto& s=ring_[(oldest+i)%Capacity];
        simulation[i]=s.simulation_end_ns-s.simulation_begin_ns;
        submit[i]=s.present_return_ns-s.render_submit_ns;
        cpu[i]=s.present_return_ns-s.simulation_begin_ns;
        if(s.input_received_ns)
            input[input_count++]=s.present_return_ns-s.input_received_ns;
    }
    result.simulation=describe(simulation,count_);
    result.submit_to_present_return=describe(submit,count_);
    result.cpu_frame=describe(cpu,count_);
    result.input_to_present_return=describe(input,input_count);
    return result;
}
std::uint64_t monotonic_ns() noexcept {
#ifdef _WIN32
    LARGE_INTEGER ticks{},frequency{};
    if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0 ||
       !QueryPerformanceCounter(&ticks) || ticks.QuadPart<=0)return 0;
    const auto f=static_cast<std::uint64_t>(frequency.QuadPart);
    const auto t=static_cast<std::uint64_t>(ticks.QuadPart);
    if(f>std::numeric_limits<std::uint64_t>::max()/Billion)return 0;
    const std::uint64_t seconds=t/f;
    if(seconds>std::numeric_limits<std::uint64_t>::max()/Billion)return 0;
    // Divide before multiplying to avoid overflow on hosts with long uptime.
    return seconds*Billion+(t%f)*Billion/f;
#else
    timespec time{};
    if(clock_gettime(CLOCK_MONOTONIC,&time)!=0 || time.tv_sec<0 ||
       time.tv_nsec<0 || time.tv_nsec>=static_cast<long>(Billion))return 0;
    const auto seconds=static_cast<std::uint64_t>(time.tv_sec);
    if(seconds>std::numeric_limits<std::uint64_t>::max()/Billion)return 0;
    return seconds*Billion+static_cast<std::uint64_t>(time.tv_nsec);
#endif
}
} // namespace scarlett::timing
