// Original portable native Windows/Linux asset-read baseline: no console assets.
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace scarlett::stream {
constexpr std::size_t MaxRequestBytes = 4u * 1024u * 1024u;
enum class Status { Ok, Cancelled, InvalidRange, FileError, ShortRead, QueueFull, Stopped };
struct Request {
    std::filesystem::path file;
    std::uint64_t offset = 0;
    std::size_t length = 0;
    std::shared_ptr<std::atomic_bool> cancelled;
};
struct Result {
    Status status = Status::FileError;
    std::vector<std::uint8_t> bytes;
    std::uint64_t checksum = 0;
};
std::uint64_t fnv1a64(const std::vector<std::uint8_t>& bytes);

// Bounded native OS thread pool, portable std::ifstream baseline.
// Future DirectStorage and Linux io_uring adapters must implement the same
// validated semantics. Neither is enabled or emulated by this class.
class Reader {
public:
    explicit Reader(std::size_t workers=2,std::size_t max_queued=64);
    ~Reader();
    Reader(const Reader&)=delete;
    Reader& operator=(const Reader&)=delete;
    std::future<Result> submit(Request request);
private:
    void work();
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::packaged_task<Result()>> pending_;
    std::vector<std::thread> workers_;
    std::size_t max_queued_;
    bool stopping_=false;
};
} // namespace scarlett::stream
