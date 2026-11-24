// Original finite, caller-invoked parallel preparation of independent PSOs.
#pragma once
#include "native_pipeline_build.hpp"
#include "native_pipeline_priority.hpp"
namespace scarlett::pipeline {
struct BatchRequest { Bytes key; std::size_t estimated_bytes=0; Priority priority=Priority::Normal; };
struct BatchLimits { std::size_t workers=4, estimated_bytes=256u*1024u*1024u; };
enum class BatchStatus { Invalid, BudgetExceeded, Completed, CallbackException, Suppressed };
struct BatchItem { BatchStatus status=BatchStatus::Invalid; BuildReport build{}; bool coalesced=false; bool failure_remembered=false; };
struct BatchResult {
    std::vector<BatchItem> items;
    std::size_t unique_jobs=0, waves=0, peak_wave_jobs=0, peak_estimated_bytes=0, suppressed_jobs=0;
};
using BatchCreate=std::function<Attempt(const Bytes& key,const Bytes* cached)>;
// Finite synchronous batch: joins all workers before returning. No persistent
// pool, scheduling, or implicit disk I/O. Cache is read-only during each wave,
// then updated on the caller thread in admission order. The callback must be safe
// for concurrent calls on distinct keys, must not access/mutate the cache, and
// owns any native object. Duplicates share one callback sequence; their largest
// declared estimate and highest priority are used. Equal priorities retain
// first-occurrence order. Callback order inside a wave is unspecified. Estimated memory is admission accounting, NOT a
// process/driver RAM limit. Successful native objects remain caller-owned.
// Invalid per-item keys/zero estimates/invalid priorities are refused. Oversized batches or invalid
// global limits throw before any callbacks; thread creation failures propagate
// after joining started workers without invoking the failed wave (earlier
// waves may already have committed).
BatchResult prepare_batch(Cache& cache,const std::vector<BatchRequest>& requests,
                          const BatchCreate& create,BatchLimits limits={});
}
