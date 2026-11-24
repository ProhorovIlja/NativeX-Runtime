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
};
// Cache ownership is bounded. Shared immutable results held by callers survive
// eviction. An optional live payload limit also accounts for external owners.
// Does not coalesce concurrent misses.
class AssetCache {
public:
    explicit AssetCache(std::size_t byte_limit, std::size_t entry_limit=1024,
                        std::size_t live_limit=0);
    std::shared_ptr<const Result> find(const CacheKey& key);
    CacheInsert insert(const CacheKey& key, Result result);
    // Successful publication returns the exact immutable buffer, even if it
    // is immediately evicted. TooLarge returns an uncached valid buffer.
    CachePublication publish(const CacheKey& key, Result result);
    std::size_t invalidate(const std::string& asset);
    void clear();
    CacheStats stats() const;
private:
    struct LiveState;
    struct TrackedResult;
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
    std::size_t peak_live_bytes_=0;
    std::uint64_t live_rejections_=0;
    std::size_t resident_bytes_=0;
    std::uint64_t hits_=0, misses_=0, evictions_=0;
};
} // namespace scarlett::stream
