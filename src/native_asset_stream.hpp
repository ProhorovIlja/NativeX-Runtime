// Original portable native Windows/Linux asset-read baseline: no console assets.
#pragma once
#include <array>
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
// Priority affects queue ordering only; the OS I/O and CPU decoding remain
// ordinary portable native calls. Background work is never completely starved
// by a continuously replenished high-priority queue.
enum class Priority : std::uint8_t { FrameCritical=0, Normal=1, Background=2 };
struct QueueChoice {
    bool found=false;
    Priority priority=Priority::Normal;
    std::size_t next_slot=0;
};
// Pure deterministic weighted fair lane chooser. When all lanes are ready,
// cycle weight is FrameCritical 4, Normal 2, Background 1. Reuse the returned
// next_slot across decisions to guarantee bounded service for nonempty lanes.
QueueChoice select_next_queue(const std::array<std::size_t,3>& counts,
                              std::size_t cursor) noexcept;
struct Request {
    std::filesystem::path file;
    std::uint64_t offset = 0;
    std::size_t length = 0;
    std::shared_ptr<std::atomic_bool> cancelled;
    Priority priority=Priority::Normal; // existing 4-field aggregates stay valid
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
    std::array<std::deque<std::packaged_task<Result()>>,3> pending_{};
    std::vector<std::thread> workers_;
    std::size_t max_queued_;
    std::size_t pending_count_=0;
    std::size_t schedule_slot_=0;
    bool stopping_=false;
};
} // namespace scarlett::stream
