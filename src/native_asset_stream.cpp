#include "native_asset_stream.hpp"
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
    if(request.length>MaxRequestBytes)return immediate(Status::InvalidRange);
    std::packaged_task<Result()> task([r=std::move(request)]{return read_exact(r);});
    auto response=task.get_future();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(stopping_)return immediate(Status::Stopped);
        if(pending_.size()>=max_queued_)return immediate(Status::QueueFull);
        pending_.push_back(std::move(task));
    }
    wake_.notify_one();
    return response;
}
void Reader::work() {
    while(true) {
        std::packaged_task<Result()> next;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait(lock,[this]{return stopping_||!pending_.empty();});
            if(pending_.empty()&&stopping_)return;
            next=std::move(pending_.front());
            pending_.pop_front();
        }
        next();
    }
}
} // namespace scarlett::stream
