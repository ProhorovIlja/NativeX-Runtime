// Original portable request coalescing for immutable, versioned assets.
#pragma once
#include "native_asset_cache.hpp"
#include <functional>

namespace scarlett::stream {
struct FlightStats {
    std::size_t active=0, limit=0;
    std::uint64_t leader_reads=0, joined=0, cache_hits=0, rejected=0;
};
// Synchronous caller-side adapter; no threads of its own. Reader (or supplied
// backend) must outlive the adapter and all calls. Backend must support
// concurrent calls for distinct flights and must not recurse into this adapter.
// An asset identity/generation denotes immutable bytes at the supplied path;
// callers must change generation if those bytes change. Different priorities
// never join the same in-flight request (no implicit priority demotion).
class CoalescedReader {
public:
    using ReadOperation=std::function<Result(Request)>;
    CoalescedReader(Reader& reader,std::size_t cache_bytes,
                    std::size_t entries=1024,std::size_t flight_limit=64,
                    std::size_t live_limit=0);
    CoalescedReader(ReadOperation read,std::size_t cache_bytes,
                    std::size_t entries=1024,std::size_t flight_limit=64,
                    std::size_t live_limit=0);
    // Cancellation is checked before and after waiting; it never cancels
    // another subscriber's read. This is not an interruptible wait API.
    std::shared_ptr<const Result> read(const CacheKey& key,
                const std::filesystem::path& path,Priority priority=Priority::Normal,
                std::shared_ptr<std::atomic_bool> cancelled={});
    FlightStats stats() const;
    CacheStats cache_stats() const {return cache_.stats();}
private:
    using Value=std::shared_ptr<const Result>;
    struct FlightKey {
        CacheKey key;
        Priority priority;
        bool operator<(const FlightKey& rhs) const noexcept;
    };
    struct Flight {
        std::promise<Value> promise;
        std::shared_future<Value> future=promise.get_future().share();
    };
    ReadOperation backend_;
    AssetCache cache_;
    const std::size_t limit_;
    mutable std::mutex mutex_;
    std::map<FlightKey,std::shared_ptr<Flight>> flights_;
    std::uint64_t leader_reads_=0, joined_=0, cache_hits_=0, rejected_=0;
};
} // namespace scarlett::stream
