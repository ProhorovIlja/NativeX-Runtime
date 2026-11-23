// Original allocation-free Windows/Linux frame phase telemetry for local profiling.
// Timestamp "Present returned" is NOT monitor scanout or input-to-photon latency.
// This library never starts monitoring, a service, recording or network I/O.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace scarlett::timing {
struct Phases {
    std::uint64_t input_received_ns=0; // 0: no input received during this frame
    std::uint64_t simulation_begin_ns=0;
    std::uint64_t simulation_end_ns=0;
    std::uint64_t render_submit_ns=0;
    std::uint64_t present_return_ns=0;
};
struct Metric {
    std::size_t count=0;
    std::uint64_t minimum_ns=0,median_ns=0,p95_ns=0,p99_ns=0,maximum_ns=0;
};
// Host-side intervals between successive returns from Present. They measure
// software call-return cadence, NOT monitor scanout, actual game FPS or latency.
// Missing and reversed/torn intervals are excluded from all percentiles.
struct PresentCadence {
    Metric interval{};
    std::size_t missed_30hz=0;
    std::size_t missed_60hz=0;
    std::size_t missed_120hz=0;
    std::size_t invalid_intervals=0; // duplicate/reversed or >60s discontinuity
};
struct Summary {
    PresentCadence present_cadence{};
    Metric simulation{};
    Metric submit_to_present_return{};
    Metric cpu_frame{};
    Metric input_to_present_return{}; // software proxy, NEVER physical photon
    std::uint64_t overwritten=0;
    std::uint64_t rejected=0;
};
class Timeline {
public:
    static constexpr std::size_t Capacity=256;
    // One owner thread writes/reads; not thread safe. Fixed memory, zero allocations.
    bool record(const Phases& sample) noexcept;
    Summary summarize() const noexcept;
    std::size_t count() const noexcept {return count_;}
    std::uint64_t rejected() const noexcept {return rejected_;}
    std::uint64_t overwritten() const noexcept {return overwritten_;}
private:
    std::array<Phases,Capacity> ring_{};
    std::size_t write_=0;
    std::size_t count_=0;
    std::uint64_t rejected_=0;
    std::uint64_t overwritten_=0;
};
// Monotonic host receipt clock with nanosecond display units.
// Resolution differs by OS/hardware; no precision guarantee at 1 ns.
std::uint64_t monotonic_ns() noexcept;
} // namespace scarlett::timing
