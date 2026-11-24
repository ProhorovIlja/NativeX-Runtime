// Independent bounded cache for completed native reads. No format-specific code.
#pragma once
#include "native_asset_stream.hpp"
#include <list>
#include <map>
#include <string>

namespace scarlett::stream {
// Caller must change generation whenever the underlying content changes.
// Asset identity is opaque and case-sensitive; no filesystem access is done here.
struct CacheKey {
    std::string asset;
    std::uint64_t generation=0;
    std::uint64_t offset=0;
    std::size_t length=0;
    bool operator<(const CacheKey& other) const noexcept;
};
enum class CacheInsert { Inserted, AlreadyPresent, Conflict, Invalid, TooLarge, LiveBudgetExceeded };
struct CachePublication {
    CacheInsert status;
    std::shared_ptr<const Result> result;
};
struct CacheStats {
    std::size_t entries=0, resident_bytes=0, byte_limit=0, entry_limit=0;
    std::uint64_t hits=0, misses=0, evictions=0;
    std::size_t live_bytes=0, peak_live_bytes=0, live_limit=0;
    std::uint64_t live_rejections=0;
    std::size_t reserved_bytes=0, committed_bytes=0, peak_committed_bytes=0;
};
// Cache ownership is bounded. Shared immutable results held by callers survive
// eviction. An optional live payload limit also accounts for external owners.
// Does not coalesce concurrent misses.
class AssetCache {
    struct LiveState;
public:
    // Move-only reservation. A valid zero-byte reservation is distinct from
    // denial. Credit returns on destruction, including after cache lifetime.
    class Reservation {
    public:
        Reservation() noexcept = default;
        ~Reservation();
        Reservation(Reservation&& other) noexcept;
        Reservation& operator=(Reservation&& other) noexcept;
        Reservation(const Reservation&)=delete;
        Reservation& operator=(const Reservation&)=delete;
        explicit operator bool() const noexcept {return bool(state_);}
    private:
        friend class AssetCache;
        Reservation(std::shared_ptr<LiveState> state,std::size_t bytes) noexcept;
        void release() noexcept;
        std::shared_ptr<LiveState> state_;
        std::size_t bytes_=0;
    };
    explicit AssetCache(std::size_t byte_limit, std::size_t entry_limit=1024,
                        std::size_t live_limit=0);
    std::shared_ptr<const Result> find(const CacheKey& key);
    CacheInsert insert(const CacheKey& key, Result result);
    // Successful publication returns the exact immutable buffer, even if it
    // is immediately evicted. TooLarge returns an uncached valid buffer.
    CachePublication publish(const CacheKey& key, Result result);
    // Reserve requested bytes before I/O. The ticket is consumed by
    // publish_reserved, even on failure or duplicate publication.
    Reservation reserve(std::size_t bytes);
    CachePublication publish_reserved(const CacheKey& key, Result result, Reservation ticket);
    std::size_t invalidate(const std::string& asset);
    void clear();
    CacheStats stats() const;
private:
    struct TrackedResult;
    Reservation reserve_locked(std::size_t bytes);
    CachePublication publish_impl(const CacheKey& key, Result result, Reservation ticket);
    struct Entry {
        std::shared_ptr<const Result> result;
        std::list<CacheKey>::iterator lru;
    };
    using Index=std::map<CacheKey,Entry>;
    void touch(Index::iterator item);
    void erase(Index::iterator item);
    mutable std::mutex mutex_;
    std::list<CacheKey> lru_; // most recently used first
    Index index_;
    const std::size_t byte_limit_, entry_limit_;
    std::shared_ptr<LiveState> live_;
    const std::size_t live_limit_; // zero: unlimited
    std::uint64_t live_rejections_=0;
    std::size_t resident_bytes_=0;
    std::uint64_t hits_=0, misses_=0, evictions_=0;
};
} // namespace scarlett::stream
