#include "native_frame_timeline.hpp"
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
    }catch(const std::exception& ex){
        std::cerr<<"native frame timeline FAILED: "<<ex.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"native-frame-phase-percentiles\","
             "\"scenarios\":5,"
             "\"fixed_capacity\":"<<Timeline::Capacity<<","
             "\"windows_and_linux_clock\":true,"
             "\"p50_p95_p99_tested\":true,"
             "\"real_game_frame_times_tested\":false,"
             "\"input_to_photon_latency_tested\":false}\n";
    return 0;
}
