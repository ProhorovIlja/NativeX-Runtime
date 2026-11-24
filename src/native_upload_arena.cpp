#include "native_upload_arena.hpp"
#include "native_texture_upload.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace scarlett::upload {
namespace {constexpr auto invalid_fence=std::numeric_limits<std::uint64_t>::max();}
UploadArena::UploadArena(std::size_t capacity,std::size_t max_allocations)
    :max_allocations_(max_allocations) {
    if(!capacity||capacity>MaxStagingBytes||!max_allocations||max_allocations>4096)
        throw std::invalid_argument("invalid upload arena limits");
    stats_.capacity=capacity;
}
std::optional<Slice> UploadArena::reserve(std::size_t bytes,std::size_t alignment) {
    if(!bytes||!alignment||alignment>65536||(alignment&(alignment-1))||
       bytes>stats_.capacity-stats_.used||entries_.size()>=max_allocations_||next_id_==invalid_fence)
        return {};
    // First-fit over sorted live intervals, including alignment gaps. Extents
    // are bounded by capacity, so rounding cannot overflow size_t.
    std::size_t cursor=0;
    const auto align=[alignment](std::size_t n){return (n+alignment-1)&~(alignment-1);};
    for(const auto& item:entries_){
        if(cursor<=item.first&&bytes<=item.first-cursor)break;
        cursor=align(item.first+item.second.slice.bytes);
    }
    if(cursor>stats_.capacity||bytes>stats_.capacity-cursor)return {};
    Slice s{cursor,bytes,next_id_};
    entries_.emplace(cursor,Entry{s,0}); // allocation before accounting
    ++next_id_;stats_.used+=bytes;stats_.reserved+=bytes;++stats_.allocations;
    stats_.peak=std::max(stats_.peak,stats_.used);
    return s;
}
UploadArena::Entries::iterator UploadArena::lookup(Slice s){
    auto it=entries_.find(s.offset);
    if(it==entries_.end()||it->second.slice.id!=s.id||it->second.slice.bytes!=s.bytes)return entries_.end();
    return it;
}
bool UploadArena::cancel(Slice s){
    auto it=lookup(s);if(it==entries_.end()||it->second.fence)return false;
    stats_.used-=s.bytes;stats_.reserved-=s.bytes;--stats_.allocations;entries_.erase(it);return true;
}
bool UploadArena::seal(Slice s,std::uint64_t fence){
    auto it=lookup(s);
    if(it==entries_.end()||it->second.fence||fence==invalid_fence||fence<=stats_.completed||fence<stats_.last_submitted)return false;
    it->second.fence=fence;stats_.last_submitted=fence;
    stats_.reserved-=s.bytes;stats_.in_flight+=s.bytes;return true;
}
bool UploadArena::collect(std::uint64_t completed){
    if(completed==invalid_fence||completed<stats_.completed)return false;
    stats_.completed=completed;
    for(auto it=entries_.begin();it!=entries_.end();){
        if(it->second.fence&&it->second.fence<=completed){
            const auto bytes=it->second.slice.bytes;stats_.used-=bytes;stats_.in_flight-=bytes;
            --stats_.allocations;it=entries_.erase(it);
        }else ++it;
    }
    return true;
}
} // namespace scarlett::upload
