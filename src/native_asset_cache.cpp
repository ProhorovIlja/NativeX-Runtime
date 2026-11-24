#include "native_asset_cache.hpp"
#include <limits>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace scarlett::stream {
namespace {
bool valid(const CacheKey& key) {
    return !key.asset.empty() && key.asset.size()<=1024 &&
           key.length<=MaxRequestBytes &&
           key.length<=std::numeric_limits<std::uint64_t>::max()-key.offset;
}
}
bool CacheKey::operator<(const CacheKey& other) const noexcept {
    return std::tie(asset,generation,offset,length)<
           std::tie(other.asset,other.generation,other.offset,other.length);
}
struct AssetCache::LiveState {
    std::atomic<std::size_t> bytes{0};
};
struct AssetCache::TrackedResult {
    // State stays alive after destruction of the cache itself. The aliasing
    // shared_ptr returned to callers keeps this owner alive until final release.
    std::shared_ptr<LiveState> state;
    Result result;
    const std::size_t size;
    TrackedResult(std::shared_ptr<LiveState> s, Result&& r) noexcept
        :state(std::move(s)),result(std::move(r)),size(result.bytes.size()) {}
    ~TrackedResult() {
        std::vector<std::uint8_t>().swap(result.bytes);
        state->bytes.fetch_sub(size);
    }
};
AssetCache::AssetCache(std::size_t byte_limit,std::size_t entry_limit,std::size_t live_limit)
    :byte_limit_(byte_limit),entry_limit_(entry_limit),live_(std::make_shared<LiveState>()),
     live_limit_(live_limit) {
    if(!byte_limit||!entry_limit||entry_limit>65536||(live_limit&&live_limit<byte_limit))
        throw std::invalid_argument("invalid asset cache limits");
}
void AssetCache::touch(Index::iterator item) {
    lru_.splice(lru_.begin(),lru_,item->second.lru);
}
void AssetCache::erase(Index::iterator item) {
    resident_bytes_-=item->second.result->bytes.size();
    lru_.erase(item->second.lru);
    index_.erase(item);
}
std::shared_ptr<const Result> AssetCache::find(const CacheKey& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto item=valid(key)?index_.find(key):index_.end();
    if(item==index_.end()){++misses_;return {};}
    ++hits_;
    touch(item);
    return item->second.result;
}
CacheInsert AssetCache::insert(const CacheKey& key, Result result) {
    return publish(key,std::move(result)).status;
}
CachePublication AssetCache::publish(const CacheKey& key, Result result) {
    if(!valid(key)||result.status!=Status::Ok||result.bytes.size()!=key.length||
       result.checksum!=fnv1a64(result.bytes))return {CacheInsert::Invalid,{}};
    // Right-size even caller-provided vectors with excess capacity. The cap
    // counts payload size, not allocator metadata or temporary allocations.
    if(result.bytes.capacity()!=result.bytes.size()) {
        std::vector<std::uint8_t> exact(result.bytes.begin(),result.bytes.end());
        result.bytes.swap(exact);
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing=index_.find(key);
    if(existing!=index_.end()) {
        if(existing->second.result->bytes!=result.bytes)return {CacheInsert::Conflict,{}};
        touch(existing);
        return {CacheInsert::AlreadyPresent,existing->second.result};
    }
    const auto cap=live_limit_?live_limit_:std::numeric_limits<std::size_t>::max();
    if(key.length>cap){++live_rejections_;return {CacheInsert::LiveBudgetExceeded,{}};}
    // Reclaim only entries currently owned exclusively by the cache. A weak
    // reference may gain ownership concurrently: recheck the actual counter.
    // Pressure evictions may occur even if admission ultimately fails.
    while(live_->bytes.load()>cap-key.length) {
        auto victim=index_.end();
        for(auto it=lru_.rbegin();it!=lru_.rend();++it) {
            auto candidate=index_.find(*it);
            if(candidate->second.result.use_count()==1 &&
               !candidate->second.result->bytes.empty()){victim=candidate;break;}
        }
        if(victim==index_.end()){++live_rejections_;return {CacheInsert::LiveBudgetExceeded,{}};}
        erase(victim);++evictions_;
    }
    // Admissions serialize under mutex_; other threads can only release credit.
    const auto old=live_->bytes.fetch_add(key.length);
    if(old+key.length>peak_live_bytes_)peak_live_bytes_=old+key.length;
    std::shared_ptr<TrackedResult> owner;
    try { owner=std::make_shared<TrackedResult>(live_,std::move(result)); }
    catch(...) { live_->bytes.fetch_sub(key.length);throw; }
    std::shared_ptr<const Result> value(owner,&owner->result);
    owner.reset();
    if(key.length>byte_limit_)return {CacheInsert::TooLarge,std::move(value)};
    // Allocate index/list nodes before ordinary LRU eviction. Pressure
    // eviction above is deliberate, even if later allocation fails.
    lru_.push_front(key);
    try { index_.emplace(key,Entry{value,lru_.begin()}); }
    catch(...) { lru_.pop_front(); throw; }
    while(resident_bytes_>byte_limit_-key.length||index_.size()>entry_limit_) {
        // Newly inserted key is MRU; it fits alone and cannot be the victim.
        erase(index_.find(lru_.back()));
        ++evictions_;
    }
    resident_bytes_+=key.length;
    return {CacheInsert::Inserted,std::move(value)};
}
std::size_t AssetCache::invalidate(const std::string& asset) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::size_t count=0;
    for(auto it=index_.begin();it!=index_.end();){
        if(it->first.asset==asset){auto victim=it++;erase(victim);++count;}
        else ++it;
    }
    return count;
}
void AssetCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    index_.clear();lru_.clear();resident_bytes_=0;
}
CacheStats AssetCache::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {index_.size(),resident_bytes_,byte_limit_,entry_limit_,hits_,misses_,evictions_,
            live_->bytes.load(),peak_live_bytes_,live_limit_,live_rejections_};
}
} // namespace scarlett::stream
