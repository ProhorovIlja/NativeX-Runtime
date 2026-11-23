// Native Windows/Linux read-only, non-destructive synthetic asset-overlay planner.
// No GTA VI paths, proprietary formats, executable loading or game integration.
#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace scarlett::overlay {
enum class Status {
    Ok, InvalidPath, ExecutablePath, SourceMissing, SymlinkRejected,
    OutsideRoot, NotRegularFile, CapacityReached
};
enum class Origin { Original, Override };
struct Result {
    Status status=Status::InvalidPath;
    Origin origin=Origin::Original;
    std::filesystem::path path{};
};

class Catalog {
public:
    static constexpr std::size_t MaxEntries=512;
    Catalog(std::filesystem::path original_root,std::filesystem::path override_root);
    // Last registered valid override wins. Source must exist and be a plain
    // file beneath the given override root when registered AND when resolved.
    Status add(std::string_view virtual_path,std::string_view source_relative);
    Result resolve(std::string_view virtual_path) const;
    std::size_t count() const noexcept {return entries_.size();}
    void clear() noexcept {entries_.clear();}
private:
    struct Entry {std::string key;std::string source_relative;};
    std::filesystem::path original_root_,override_root_;
    std::vector<Entry> entries_{};
};
const char* name(Status v) noexcept;
}
