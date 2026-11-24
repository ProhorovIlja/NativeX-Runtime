#include "native_asset_singleflight.hpp"
#include <limits>
#include <stdexcept>
#include <utility>

namespace scarlett::stream {
namespace {
std::shared_ptr<const Result> failure(Status status) {
    return std::make_shared<const Result>(Result{status,{},0});
}
bool is_cancelled(const std::shared_ptr<std::atomic_bool>& token) {
    return token && token->load();
}
}
bool CoalescedReader::FlightKey::operator<(const FlightKey& rhs) const noexcept {
    if(key<rhs.key)return true;
    if(rhs.key<key)return false;
    return priority<rhs.priority;
}
CoalescedReader::CoalescedReader(Reader& reader,std::size_t cache_bytes,
                                 std::size_t entries,std::size_t flight_limit,std::size_t live_limit)
    :CoalescedReader([&reader](Request request){return reader.submit(std::move(request)).get();},
                     cache_bytes,entries,flight_limit,live_limit) {}
CoalescedReader::CoalescedReader(ReadOperation read,std::size_t cache_bytes,
                                 std::size_t entries,std::size_t flight_limit,std::size_t live_limit)
    :backend_(std::move(read)),cache_(cache_bytes,entries,live_limit),limit_(flight_limit) {
    if(!backend_||!limit_||limit_>256)throw std::invalid_argument("invalid single-flight configuration");
}
std::shared_ptr<const Result> CoalescedReader::read(const CacheKey& key,
                const std::filesystem::path& path,Priority priority,
                std::shared_ptr<std::atomic_bool> cancelled) {
    if(key.asset.empty()||key.asset.size()>1024||key.length>MaxRequestBytes||
       key.length>std::numeric_limits<std::uint64_t>::max()-key.offset||
       static_cast<std::size_t>(priority)>2u||path.empty())return failure(Status::InvalidRange);
    if(is_cancelled(cancelled))return failure(Status::Cancelled);
    const FlightKey identity{key,priority};
    std::shared_ptr<Flight> flight;
    bool leader=false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(auto found=cache_.find(key)) {
            ++cache_hits_;
            return is_cancelled(cancelled)?failure(Status::Cancelled):found;
        }
        const auto existing=flights_.find(identity);
        if(existing!=flights_.end()) {
            flight=existing->second;++joined_;
        } else {
            if(flights_.size()>=limit_){++rejected_;return failure(Status::QueueFull);}
            flight=std::make_shared<Flight>();
            flights_.emplace(identity,flight);
            ++leader_reads_;leader=true;
        }
    }
    if(leader) {
        Value value;
        std::exception_ptr error;
        try {
            // Subscriber cancellation is deliberately not passed to shared I/O.
            auto result=backend_(Request{path,key.offset,key.length,{},priority});
            if(result.status==Status::Ok) {
                auto published=cache_.publish(key,std::move(result));
                if(published.status==CacheInsert::LiveBudgetExceeded)
                    value=failure(Status::BudgetExceeded);
                else {
                    if(!published.result)throw std::runtime_error("invalid or conflicting immutable asset result");
                    value=std::move(published.result);
                }
            } else {
                value=failure(result.status);
            }
        } catch(...) {error=std::current_exception();}
        // Settle and retire together under the same lock. Failed reads are
        // never cached; later callers may retry, including after exceptions.
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if(error)flight->promise.set_exception(error);
            else flight->promise.set_value(std::move(value));
            flights_.erase(identity);
        }
    }
    if(is_cancelled(cancelled))return failure(Status::Cancelled);
    // If the backend throws, every uncancelled subscriber observes it.
    try {
        auto value=flight->future.get();
        return is_cancelled(cancelled)?failure(Status::Cancelled):value;
    } catch(...) {
        if(is_cancelled(cancelled))return failure(Status::Cancelled);
        throw;
    }
}
FlightStats CoalescedReader::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {flights_.size(),limit_,leader_reads_,joined_,cache_hits_,rejected_};
}
} // namespace scarlett::stream
