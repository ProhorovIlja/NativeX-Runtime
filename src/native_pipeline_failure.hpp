// Explicit, bounded, process-local negative cache for deterministic translation refusal.
#pragma once
#include "native_pipeline_batch.hpp"
#include <stdexcept>
#include <utility>
namespace scarlett::pipeline {
// Not inferred from HRESULTs, timeouts, allocation failures or device removal.
// Only a translator's reproducible unsupported operation may set this class.
enum class FailureClass { None, DeterministicUnsupported };
struct ClassifiedAttempt { Attempt attempt; FailureClass failure=FailureClass::None; };
using ClassifiedCreate=std::function<ClassifiedAttempt(const Bytes*)>;
using ClassifiedBatchCreate=std::function<ClassifiedAttempt(const Bytes&,const Bytes*)>;
struct GuardReport { BuildReport build{}; bool suppressed=false, remembered=false; };
// Single owner, no reentrant callbacks. Instance lifetime defines compatibility
// scope (adapter, driver, translator, feature flags). No disk persistence of failures.
// Exact keys, bounded payload bytes and entries; full cache declines new records.
class GuardedPipelineCache {
public:
    explicit GuardedPipelineCache(Bytes scope,std::size_t failure_entries=1024,
                                  std::size_t failure_key_bytes=8u*1024u*1024u,
                                  std::size_t blob_budget=MaxSnapshotBytes)
      : cache_(std::move(scope),blob_budget),limit_(failure_entries),budget_(failure_key_bytes) {
        if(limit_>4096||budget_>MaxSnapshotBytes)throw std::invalid_argument("invalid failure limits");
    }
    GuardReport create(const Bytes& key,const ClassifiedCreate& op) {
        GuardReport report;
        if(key.empty()||key.size()>MaxKeyBytes||!op)return report;
        if(known(key)){
            report.suppressed=true;report.build.path=BuildPath::Failed;return report;
        }
        bool permanent=false;
        report.build=create_cached(cache_,key,[&](const Bytes* cached){
            auto a=op(cached);
            permanent=!cached&&a.attempt.status==AttemptStatus::Failed&&a.failure==FailureClass::DeterministicUnsupported;
            return std::move(a.attempt);
        });
        if(permanent&&report.build.path==BuildPath::Failed)report.remembered=remember(key);
        return report;
    }
    bool forget(const Bytes& key){auto i=failures_.find(key);if(i==failures_.end())return false;bytes_-=i->first.size();failures_.erase(i);return true;}
    void clear_failures() noexcept {failures_.clear();bytes_=0;}
    std::size_t failure_count() const noexcept{return failures_.size();}
    std::size_t failure_key_bytes() const noexcept{return bytes_;}
    // Successful snapshot loading changes available blobs, so reset failures.
    Status load(const std::filesystem::path& file){auto s=cache_.load(file);if(s==Status::Ok)clear_failures();return s;}
    Status save(const std::filesystem::path& file) const{return cache_.save(file);}
private:
    friend BatchResult prepare_guarded_batch(GuardedPipelineCache&,const std::vector<BatchRequest>&,const ClassifiedBatchCreate&,BatchLimits);
    bool known(const Bytes& key) const {return failures_.find(key)!=failures_.end();}
    bool remember(const Bytes& key){
        if(failures_.size()>=limit_||key.size()>budget_-bytes_)return false;
        if(!failures_.emplace(key,true).second)return false;
        bytes_+=key.size();return true;
    }
    Cache cache_;
    std::map<Bytes,bool> failures_;
    std::size_t limit_,budget_,bytes_=0;
};
// Same finite admission rules as prepare_batch. Known failures consume no
// worker or estimated-byte budget; invalid requests still return Invalid.
// Negative cache reads/writes stay on the caller thread.
BatchResult prepare_guarded_batch(GuardedPipelineCache& cache,const std::vector<BatchRequest>& requests,
                                  const ClassifiedBatchCreate& create,BatchLimits limits={});
}
