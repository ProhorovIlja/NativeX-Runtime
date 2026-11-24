// Original single-queue upload suballocator. No OS/GPU dependency.
#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
namespace scarlett::upload {
struct Slice { std::size_t offset=0, bytes=0; std::uint64_t id=0; };
struct ArenaStats {
    std::size_t capacity=0, used=0, peak=0, reserved=0, in_flight=0, allocations=0;
    std::uint64_t completed=0, last_submitted=0;
};
// One owner thread and one monotonically increasing GPU fence timeline.
// Slice tokens belong to this instance only. Actual memory and command queue
// are owned externally. No waits, GPU calls, or implicit retirement.
class UploadArena {
public:
    explicit UploadArena(std::size_t capacity,std::size_t max_allocations=1024);
    UploadArena(const UploadArena&)=delete;
    UploadArena& operator=(const UploadArena&)=delete;
    std::optional<Slice> reserve(std::size_t bytes,std::size_t alignment=512);
    bool cancel(Slice slice); // only before any GPU use
    // Seal before submitting GPU commands, with the fence value to signal
    // after their last use. Failed GPU submission/signal requires external
    // recovery; never pretend the fence has completed.
    bool seal(Slice slice,std::uint64_t fence);
    // Pass observed GPU completion only. UINT64_MAX (device loss) is rejected.
    bool collect(std::uint64_t completed);
    ArenaStats stats() const noexcept {return stats_;}
private:
    struct Entry { Slice slice; std::uint64_t fence=0; };
    using Entries=std::map<std::size_t,Entry>;
    Entries::iterator lookup(Slice slice);
    Entries entries_;
    ArenaStats stats_;
    const std::size_t max_allocations_;
    std::uint64_t next_id_=1;
};
} // namespace scarlett::upload
