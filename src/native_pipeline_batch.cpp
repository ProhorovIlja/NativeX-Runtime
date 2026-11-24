#include "native_pipeline_failure.hpp"
#include <algorithm>
#include <map>
#include <future>
#include <stdexcept>
#include <thread>
#include <utility>
namespace scarlett::pipeline {
namespace {
constexpr std::size_t MaxRequests=4096,MaxBatchKeyBytes=16u*1024u*1024u;
constexpr std::size_t MaxEstimate=2ull*1024u*1024u*1024u;
struct Work {
    const Bytes* key=nullptr;
    std::size_t estimate=0;
    Priority priority=Priority::Normal;
    BatchItem outcome{};
    Bytes fresh;
    bool permanent=false;
};
struct Join {
    std::vector<std::thread> threads;
    ~Join(){for(auto& t:threads)if(t.joinable())t.join();}
};
void prepare(Work& w,const Bytes* cached,const ClassifiedBatchCreate& create){
    auto& o=w.outcome;auto& r=o.build;
    o.status=BatchStatus::Completed;r.path=BuildPath::Failed;
    try {
        if(cached){
            ++r.attempts;auto a=create(*w.key,cached).attempt;
            if(a.status==AttemptStatus::Created){r.path=BuildPath::Hit;return;}
            if(a.status!=AttemptStatus::CacheRejected)return;
            r.cache_rejected=true;
        }
        ++r.attempts;auto classified=create(*w.key,nullptr);
        auto& a=classified.attempt;
        w.permanent=a.status==AttemptStatus::Failed&&classified.failure==FailureClass::DeterministicUnsupported;
        if(a.status!=AttemptStatus::Created)return;
        r.path=r.cache_rejected?BuildPath::Rebuilt:BuildPath::Cold;
        if(a.fresh_blob.empty())r.write=WriteStatus::Unavailable;
        else if(a.fresh_blob.size()>MaxBlobBytes||a.fresh_blob.size()>w.estimate)r.write=WriteStatus::Rejected;
        else w.fresh=std::move(a.fresh_blob);
    }catch(...){o.status=BatchStatus::CallbackException;r.path=BuildPath::Failed;}
}
using Lookup=std::function<bool(const Bytes&)>;
BatchResult prepare_impl(Cache& cache,const std::vector<BatchRequest>& requests,
                         const ClassifiedBatchCreate& create,BatchLimits limits,
                         const Lookup& known,const Lookup& remember){
    if(!create||!limits.workers||limits.workers>32||!limits.estimated_bytes||limits.estimated_bytes>MaxEstimate||requests.size()>MaxRequests)
        throw std::invalid_argument("invalid pipeline batch limits");
    std::size_t total_keys=0;
    for(const auto& req:requests){if(req.key.size()>MaxBatchKeyBytes-total_keys)throw std::invalid_argument("batch key budget exceeded");total_keys+=req.key.size();}
    BatchResult result;result.items.resize(requests.size());
    std::map<Bytes,std::size_t> indices;std::vector<Work> work;work.reserve(requests.size());
    const auto missing=requests.size();std::vector<std::size_t> mapping(requests.size(),missing);
    for(std::size_t i=0;i<requests.size();++i){const auto& req=requests[i];
        if(req.key.empty()||req.key.size()>MaxKeyBytes||!req.estimated_bytes||!valid_priority(req.priority))continue;
        auto inserted=indices.emplace(req.key,work.size());const auto j=inserted.first->second;mapping[i]=j;
        if(inserted.second){Work w;w.key=&req.key;w.estimate=req.estimated_bytes;w.priority=req.priority;work.push_back(std::move(w));}
        else {work[j].estimate=std::max(work[j].estimate,req.estimated_bytes);work[j].priority=promote_priority(work[j].priority,req.priority);result.items[i].coalesced=true;}
    }
    result.unique_jobs=work.size();
    std::vector<Priority> priorities;priorities.reserve(work.size());
    for(const auto& w:work)priorities.push_back(w.priority);
    const auto order=priority_order(priorities);
    // Fixed finite waves: admission is deterministic and cache writes never
    // overlap reads by worker threads. A very large job does not block later jobs.
    std::size_t cursor=0;
    while(cursor<work.size()){
        std::vector<std::size_t> wave;wave.reserve(limits.workers);std::size_t bytes=0;
        while(cursor<work.size()&&wave.size()<limits.workers){auto& w=work[order[cursor]];
            if(known&&known(*w.key)){w.outcome.status=BatchStatus::Suppressed;w.outcome.build.path=BuildPath::Failed;++result.suppressed_jobs;++cursor;continue;}
            if(w.estimate>limits.estimated_bytes){w.outcome.status=BatchStatus::BudgetExceeded;++cursor;continue;}
            if(w.estimate>limits.estimated_bytes-bytes)break;
            bytes+=w.estimate;wave.push_back(order[cursor++]);
        }
        if(wave.empty())continue;
        ++result.waves;result.peak_wave_jobs=std::max(result.peak_wave_jobs,wave.size());result.peak_estimated_bytes=std::max(result.peak_estimated_bytes,bytes);
        {
            // Do not invoke a callback until every thread has started. If
            // thread creation fails, all started workers exit without work.
            std::promise<bool> start;auto ready=start.get_future().share();
            Join join;join.threads.reserve(wave.size()-1);
            try {
                for(std::size_t i=1;i<wave.size();++i){auto& w=work[wave[i]];const auto* blob=cache.find(*w.key);
                    join.threads.emplace_back([&w,blob,&create,ready]{if(ready.get())prepare(w,blob,create);});}
            }catch(...){start.set_value(false);throw;}
            start.set_value(true);
            auto& w=work[wave.front()];prepare(w,cache.find(*w.key),create);
        }
        for(auto j:wave){auto& w=work[j];auto& r=w.outcome.build;
            // A confirmed rejection invalidates the old blob even if the
            // uncached retry fails. A thrown initial callback preserves it.
            if(r.cache_rejected)cache.erase(*w.key);
            if(w.permanent&&remember)w.outcome.failure_remembered=remember(*w.key);
            if(!w.fresh.empty()){r.write=cache.put(*w.key,w.fresh)==Status::Ok?WriteStatus::Stored:WriteStatus::Rejected;Bytes{}.swap(w.fresh);}
        }
    }
    for(std::size_t i=0;i<mapping.size();++i)if(mapping[i]!=missing){const bool duplicate=result.items[i].coalesced;result.items[i]=work[mapping[i]].outcome;result.items[i].coalesced=duplicate;}
    return result;
}
}
BatchResult prepare_batch(Cache& cache,const std::vector<BatchRequest>& requests,
                          const BatchCreate& create,BatchLimits limits){
    if(!create)throw std::invalid_argument("invalid pipeline batch callback");
    return prepare_impl(cache,requests,[&](const Bytes& key,const Bytes* blob){return ClassifiedAttempt{create(key,blob),FailureClass::None};},limits,{},{});
}
BatchResult prepare_guarded_batch(GuardedPipelineCache& cache,const std::vector<BatchRequest>& requests,
                                  const ClassifiedBatchCreate& create,BatchLimits limits){
    return prepare_impl(cache.cache_,requests,create,limits,[&](const Bytes& key){return cache.known(key);},[&](const Bytes& key){return cache.remember(key);});
}
}
