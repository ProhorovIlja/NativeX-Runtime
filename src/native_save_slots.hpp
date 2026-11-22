// Original small, native Windows/Linux two-slot save-snapshot transaction fixture.
// This is NOT a GTA VI game save format or integrated autosave.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>
#include <utility>

namespace scarlett::save {
constexpr std::size_t MaxPayloadBytes = 2u*1024u*1024u;
enum class Status : std::uint8_t {
    Ok, NotFound, Corrupt, IoError, TooLarge, Conflict, InvalidState, Interrupted
};
enum class Fault : std::uint8_t { None, BeforePublish, AfterPublish };
struct Snapshot {
    std::uint64_t generation=0;
    std::vector<std::uint8_t> bytes;
};
// A single process/thread owns a Store for its whole lifetime.
// There is intentionally NO concurrent-writer arbitration or game format support.
class Store {
public:
    explicit Store(std::filesystem::path dir):directory_(std::move(dir)){}
    Status load(Snapshot& snapshot) const;
    Status commit(const std::vector<std::uint8_t>& bytes,
                  std::uint64_t expected_generation,
                  Fault fault=Fault::None);
private:
    std::filesystem::path directory_;
};
const char* status_text(Status status) noexcept;
}
