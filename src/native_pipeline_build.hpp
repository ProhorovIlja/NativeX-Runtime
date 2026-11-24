// Original compute PSO identity and explicit cached-creation fallback.
#pragma once
#include "native_pipeline_cache.hpp"
#include <functional>
namespace scarlett::pipeline {
// Canonical key for the legacy D3D12_COMPUTE_PIPELINE_STATE_DESC only.
// Serialized root signature and exact shader bytes are required; scope is
// supplied separately to Cache. No raw pointers, padding or cached blob in key.
Bytes compute_key(const Bytes& root,const Bytes& shader,std::uint32_t node_mask=0,
                  std::uint32_t flags=0);
enum class AttemptStatus { Created, CacheRejected, Failed };
struct Attempt { AttemptStatus status=AttemptStatus::Failed; Bytes fresh_blob; };
enum class BuildPath { Invalid, Cold, Hit, Rebuilt, Failed };
enum class WriteStatus { NotAttempted, Unavailable, Stored, Rejected };
struct BuildReport {
    BuildPath path=BuildPath::Invalid;
    WriteStatus write=WriteStatus::NotAttempted;
    unsigned attempts=0;
    bool cache_rejected=false;
};
// The callback owns the resulting native object; Created means it is usable.
// It may return a freshly generated blob on the uncached path. A cached hit
// retains the existing blob. CacheRejected means a specific cached-state
// incompatibility (not device loss/OOM); exactly one fresh retry is allowed.
// Single owner, no reentrancy or mutation of Cache from the callback. Borrowed
// cached bytes valid only during callback. Exceptions propagate, never retry.
using CreateOperation=std::function<Attempt(const Bytes* cached)>;
BuildReport create_cached(Cache& cache,const Bytes& key,const CreateOperation& create);
} // namespace scarlett::pipeline
