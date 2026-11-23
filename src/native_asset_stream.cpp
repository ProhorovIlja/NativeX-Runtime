#include "native_asset_stream.hpp"
#include <array>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace scarlett::stream {
namespace {
std::future<Result> immediate(Status s) {
    std::promise<Result> p;
    auto future=p.get_future();
    p.set_value(Result{s,{},0});
    return future;
}
Result read_exact(const Request& request) {
    if (request.cancelled && request.cancelled->load())return {Status::Cancelled,{},0};
    if (request.length>MaxRequestBytes ||
        request.offset>static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max()))
        return {Status::InvalidRange,{},0};
    std::ifstream source(request.file,std::ios::binary|std::ios::ate);
    if(!source.is_open())return {Status::FileError,{},0};
    const auto end=source.tellg();
    if(end<0)return {Status::FileError,{},0};
    const auto file_size=static_cast<std::uint64_t>(end);
    // Subtraction after comparison prevents offset + length integer overflow.
    if(request.offset>file_size ||
       static_cast<std::uint64_t>(request.length)>file_size-request.offset)
        return {Status::InvalidRange,{},0};
    source.seekg(static_cast<std::streamoff>(request.offset),std::ios::beg);
    if(!source)return {Status::FileError,{},0};
    Result result{Status::Ok,{},0};
    result.bytes.resize(request.length);
    if(request.length>0) {
        source.read(reinterpret_cast<char*>(result.bytes.data()),
                    static_cast<std::streamsize>(request.length));
        if(!source || source.gcount()!=static_cast<std::streamsize>(request.length))
            return {Status::ShortRead,{},0};
    }
    if(request.cancelled && request.cancelled->load())
        return {Status::Cancelled,{},0};
    result.checksum=fnv1a64(result.bytes);
    return result;
}
}
// Weighted round robin across *ready* priority queues. We never discard a
// background lane to satisfy a frame-critical lane, and a continuously busy
// frame-critical stream can use only four of seven all-ready selections.
QueueChoice select_next_queue(const std::array<std::size_t,3>& queued,
                              std::size_t cursor) noexcept {
    constexpr std::array<Priority,7> wheel{{
        Priority::FrameCritical,Priority::Normal,Priority::FrameCritical,
        Priority::Background,Priority::FrameCritical,Priority::Normal,
        Priority::FrameCritical
    }};
    const auto start=cursor%wheel.size();
    for(std::size_t offset=0;offset<wheel.size();offset++){
        const auto slot=(start+offset)%wheel.size();
        const auto lane=wheel[slot];
        if(queued[static_cast<std::size_t>(lane)]!=0)
            return {true,lane,(slot+1u)%wheel.size()};
    }
    return {false,Priority::Normal,start};
}
std::uint64_t fnv1a64(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t hash=14695981039346656037ull;
    for(const std::uint8_t b:bytes) {
        hash^=b;
        hash*=1099511628211ull;
    }
    return hash;
}
Reader::Reader(std::size_t workers,std::size_t max_queued):max_queued_(max_queued) {
    if(workers==0||workers>16||max_queued==0||max_queued>256)
        throw std::invalid_argument("invalid thread/queue configuration");
    // A failure during partial thread startup must join already launched
    // threads. Otherwise std::thread destructors would call std::terminate.
    try {
        for(std::size_t i=0;i<workers;i++)
            workers_.emplace_back([this]{work();});
    } catch(...) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_=true;
        }
        wake_.notify_all();
        for(auto& thread:workers_)if(thread.joinable())thread.join();
        throw;
    }
}
Reader::~Reader() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_=true;
    }
    wake_.notify_all();
    for(auto& worker:workers_)if(worker.joinable())worker.join();
}
std::future<Result> Reader::submit(Request request) {
    if(request.length>MaxRequestBytes ||
       static_cast<std::size_t>(request.priority)>2u)
        return immediate(Status::InvalidRange);
    const auto lane=static_cast<std::size_t>(request.priority);
    std::packaged_task<Result()> task([r=std::move(request)]{return read_exact(r);});
    auto response=task.get_future();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_)return immediate(Status::Stopped);
        if(pending_count_>=max_queued_)return immediate(Status::QueueFull);
        pending_[lane].push_back(std::move(task));
        ++pending_count_;
    }
    wake_.notify_one();
    return response;
}
void Reader::work() {
    while(true) {
        std::packaged_task<Result()> next;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock,[this]{return stopping_||pending_count_!=0;});
            if(pending_count_==0&&stopping_)return;
            const auto decision=select_next_queue({{
                pending_[0].size(),pending_[1].size(),pending_[2].size()
            }},schedule_slot_);
            // pending_count_>0 always implies an available lane.
            const auto lane=static_cast<std::size_t>(decision.priority);
            schedule_slot_=decision.next_slot;
            next=std::move(pending_[lane].front());
            pending_[lane].pop_front();
            --pending_count_;
        }
        next();
    }
}
} // namespace scarlett::stream
