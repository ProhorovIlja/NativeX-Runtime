#include "native_frame_timeline.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace scarlett::timing;
namespace {
constexpr std::uint64_t M=1000000ull;
void check(bool condition,const char* why){
    if(!condition)throw std::runtime_error(why);
}
Phases frame(std::uint64_t i,std::uint64_t duration_ms,bool with_input=true){
    const std::uint64_t begin=(i+1)*1000000000ull;
    return {with_input?begin-500000ull:0ull,begin,
            begin+duration_ms*M,
            begin+duration_ms*M+M,
            begin+duration_ms*M+2*M};
}
void empty_and_rejections(){
    Timeline t{};
    auto summary=t.summarize();
    check(!summary.cpu_frame.count && !summary.input_to_present_return.count,
          "empty timeline count");
    check(!t.record({}),"zero clocks accepted");
    check(!t.record({10,20,19,21,22}),"reversed simulation phase accepted");
    check(!t.record({30,20,25,26,28}),"input after sim start accepted");
    check(!t.record({0,20,25,24,28}),"render submit before sim end accepted");
    check(!t.record({0,20,25,28,27}),"present return before submit accepted");
    check(!t.record({0,1,2,3,70000000000ull}),"unbounded frame span accepted");
    summary=t.summarize();
    check(t.count()==0 && summary.rejected==6 && summary.overwritten==0,
          "invalid records mutated ring");
}
void exact_quantiles(){
    Timeline t{};
    for(std::uint64_t i=1;i<=100;i++)check(t.record(frame(i,i)),"valid percentile sample");
    const auto m=t.summarize();
    check(m.cpu_frame.count==100 && m.simulation.count==100,
          "100-window count");
    check(m.simulation.minimum_ns==M &&
          m.simulation.median_ns==50*M &&
          m.simulation.p95_ns==95*M &&
          m.simulation.p99_ns==99*M &&
          m.simulation.maximum_ns==100*M,"nearest-rank percentile boundaries");
    check(m.cpu_frame.median_ns==52*M && m.cpu_frame.p99_ns==101*M,
          "CPU simulation-to-Present-return quantiles");
    check(m.submit_to_present_return.median_ns==M &&
          m.submit_to_present_return.p99_ns==M,
          "submit-to-present-return sample");
    check(m.input_to_present_return.count==100 &&
          m.input_to_present_return.median_ns==52*M+500000ull,
          "software input-to-Present return proxy");
}
void bounded_ring_and_no_input(){
    Timeline t{};
    for(std::uint64_t i=1;i<=260;i++)
        check(t.record(frame(i,i,false)),"ring record failed");
    const auto m=t.summarize();
    check(t.count()==Timeline::Capacity && t.overwritten()==4 &&
          m.simulation.minimum_ns==5*M &&
          m.simulation.maximum_ns==260*M &&
          m.simulation.median_ns==132*M &&
          m.simulation.p95_ns==248*M &&
          m.simulation.p99_ns==258*M,
          "ring replacement/quantile overflow");
    check(m.input_to_present_return.count==0 &&
          m.input_to_present_return.maximum_ns==0,"no input marker accepted");
}
void strict_timestamp_bounds(){
    Timeline t{};
    check(t.record({0,1,1,1,1}),"valid zero-duration event rejected");
    check(t.record({100,100,100,100,100}),"coincident phase stamps rejected");
    const auto m=t.summarize();
    check(m.simulation.count==2 && m.simulation.maximum_ns==0 &&
          m.cpu_frame.maximum_ns==0,"same-timestamp metrics");
}

Phases present_at(std::uint64_t timestamp) {
    // A source-built sample with 2 ms of CPU work, independent of the
    // interval since the preceding Present return.
    return {0,timestamp-2000000ull,timestamp-1500000ull,
            timestamp-1000000ull,timestamp};
}
void present_return_cadence_and_30_60_120_budgets(){
    Timeline t{};
    std::uint64_t timestamp=10000000000ull;
    check(t.record(present_at(timestamp)),"initial cadence frame");
    constexpr std::array<std::uint64_t,6> gaps{{
        16666667ull,16666667ull,33333334ull,
        8333333ull,8333334ull,1000000000ull
    }};
    for(auto gap:gaps){
        timestamp+=gap;
        check(t.record(present_at(timestamp)),"valid cadence frame");
    }
    const auto m=t.summarize();
    const auto& c=m.present_cadence;
    check(c.interval.count==6 && c.interval.minimum_ns==8333333ull &&
          c.interval.median_ns==16666667ull &&
          c.interval.p95_ns==1000000000ull &&
          c.interval.p99_ns==1000000000ull &&
          c.interval.maximum_ns==1000000000ull,
          "Present-return interval quantiles changed");
    check(c.missed_30hz==1 && c.missed_60hz==2 &&
          c.missed_120hz==4 && c.invalid_intervals==0,
          "30/60/120 fps budget deadline rounding or miss count wrong");
    check(m.cpu_frame.count==7 && m.cpu_frame.p99_ns==2000000ull,
          "Present cadence incorrectly replaces CPU phase duration");
}
void cadence_rejects_pauses_and_reverse_clock(){
    Timeline t{};
    check(t.record(present_at(10000000000ull)),"start");
    check(t.record(present_at(10000000000ull)),"coincident return stamp");
    check(t.record(present_at(9999000000ull)),"reordered return stamp");
    check(t.record(present_at(80000000000ull)),"host suspension gap");
    check(t.record(present_at(80016666667ull)),"new stable interval");
    const auto c=t.summarize().present_cadence;
    check(c.invalid_intervals==3 && c.interval.count==1 &&
          c.interval.minimum_ns==16666667ull &&
          c.missed_30hz==0 && c.missed_60hz==0 &&
          c.missed_120hz==1,
          "clock duplicates, backwards returns or 60s pause fabricated cadence");
}
void cadence_bounded_ring_and_empty(){
    Timeline t{};
    check(t.summarize().present_cadence.interval.count==0,
          "empty timeline manufactured Present rate");
    constexpr auto start=10000000000ull;
    for(std::size_t i=0;i<260;i++){
        const auto when=start+static_cast<std::uint64_t>(i)*20000000ull;
        check(t.record(present_at(when)),"bounded cadence sample");
    }
    const auto m=t.summarize();
    check(t.count()==Timeline::Capacity && m.overwritten==4 &&
          m.present_cadence.interval.count==Timeline::Capacity-1u &&
          m.present_cadence.interval.median_ns==20000000ull &&
          m.present_cadence.interval.p99_ns==20000000ull &&
          m.present_cadence.missed_30hz==0 &&
          m.present_cadence.missed_60hz==Timeline::Capacity-1u &&
          m.present_cadence.missed_120hz==Timeline::Capacity-1u,
          "ring wrapped with incorrect software cadence pairs");
}
void native_monotonic_clock(){
    const std::uint64_t first=monotonic_ns();
    const std::uint64_t second=monotonic_ns();
    check(first>0 && second>=first,"native monotonic OS clock invalid");
}
}
int main(){
    try{
        empty_and_rejections();exact_quantiles();bounded_ring_and_no_input();
        strict_timestamp_bounds();native_monotonic_clock();
        present_return_cadence_and_30_60_120_budgets();
        cadence_rejects_pauses_and_reverse_clock();
        cadence_bounded_ring_and_empty();
    }catch(const std::exception& ex){
        std::cerr<<"native frame timeline FAILED: "<<ex.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"native-frame-phase-percentiles\","
             "\"scenarios\":8,"
             "\"fixed_capacity\":"<<Timeline::Capacity<<","
             "\"windows_and_linux_clock\":true,"
             "\"p50_p95_p99_tested\":true,"
             "\"real_game_frame_times_tested\":false,"
             "\"input_to_photon_latency_tested\":false}\n";
    return 0;
}
