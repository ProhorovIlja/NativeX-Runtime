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
AssetCache::AssetCache(std::size_t byte_limit,std::size_t entry_limit)
    :byte_limit_(byte_limit),entry_limit_(entry_limit) {
    if(!byte_limit||!entry_limit||entry_limit>65536)
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
    auto value=std::make_shared<const Result>(std::move(result));
    if(key.length>byte_limit_)return {CacheInsert::TooLarge,std::move(value)};
    std::lock_guard<std::mutex> lock(mutex_);
    auto existing=index_.find(key);
    if(existing!=index_.end()) {
        if(existing->second.result->bytes!=value->bytes)return {CacheInsert::Conflict,{}};
        touch(existing);
        return {CacheInsert::AlreadyPresent,existing->second.result};
    }
    // Allocate index/list nodes before evicting so allocation failure cannot
    // discard existing entries or corrupt accounting.
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
    return {index_.size(),resident_bytes_,byte_limit_,entry_limit_,hits_,misses_,evictions_};
}
} // namespace scarlett::stream
