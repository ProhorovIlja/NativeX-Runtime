// Original bounded local cache of opaque driver PSO blobs. No graphics API dependency.
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <vector>
namespace scarlett::pipeline {
using Bytes=std::vector<std::uint8_t>;
enum class Status { Ok, Miss, Invalid, Full, Corrupt, ScopeMismatch, IoError };
constexpr std::size_t MaxSnapshotBytes=64u*1024u*1024u;
constexpr std::size_t MaxKeyBytes=1024u*1024u;
constexpr std::size_t MaxBlobBytes=16u*1024u*1024u;
// Single owner thread/process. Scope must encode adapter, driver, runtime and
// compiler/translator versions. Key must encode ALL semantic pipeline state,
// including shader stages, root signatures, formats and flags, not pointers.
// Only trusted local driver-generated blobs; checksum is not authentication.
class Cache {
public:
    explicit Cache(Bytes scope,std::size_t budget=MaxSnapshotBytes,std::size_t entries=4096);
    Status put(const Bytes& key,const Bytes& blob);
    const Bytes* find(const Bytes& key) const; // borrowed until modification
    bool erase(const Bytes& key);
    std::size_t size() const noexcept {return entries_.size();}
    std::size_t snapshot_bytes() const noexcept {return bytes_;}
    // Explicit snapshot I/O only. Failed load leaves current entries intact.
    // Save uses an exclusively created temporary directory in the same parent
    // and rename; no crash-durability or concurrent writer guarantee.
    Status load(const std::filesystem::path& file);
    Status save(const std::filesystem::path& file) const;
private:
    Bytes scope_;
    std::size_t budget_,limit_,bytes_;
    std::map<Bytes,Bytes> entries_;
};
} // namespace scarlett::pipeline
