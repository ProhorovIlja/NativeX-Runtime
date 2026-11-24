// Explicit, bounded, process-local negative cache for deterministic translation refusal.
#pragma once
#include "native_pipeline_build.hpp"
#include <stdexcept>
#include <utility>
namespace scarlett::pipeline {
// Not inferred from HRESULTs, timeouts, allocation failures or device removal.
// Only a translator's reproducible unsupported operation may set this class.
enum class FailureClass { None, DeterministicUnsupported };
struct ClassifiedAttempt { Attempt attempt; FailureClass failure=FailureClass::None; };
using ClassifiedCreate=std::function<ClassifiedAttempt(const Bytes*)>;
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
        if(failures_.find(key)!=failures_.end()){
            report.suppressed=true;report.build.path=BuildPath::Failed;return report;
        }
        bool permanent=false;
        report.build=create_cached(cache_,key,[&](const Bytes* cached){
            auto a=op(cached);
            permanent=!cached&&a.attempt.status==AttemptStatus::Failed&&a.failure==FailureClass::DeterministicUnsupported;
            return std::move(a.attempt);
        });
        if(permanent&&report.build.path==BuildPath::Failed&&failures_.size()<limit_&&key.size()<=budget_-bytes_){
            if(failures_.emplace(key,true).second){bytes_+=key.size();report.remembered=true;}
        }
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
    Cache cache_;
    std::map<Bytes,bool> failures_;
    std::size_t limit_,budget_,bytes_=0;
};
}
