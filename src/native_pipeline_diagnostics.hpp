#pragma once
#include "native_pipeline_batch.hpp"
#include <string>
#include <algorithm>
namespace scarlett::pipeline {
// Explicit bounded JSON snapshot. No keys, shader bytes, paths, timestamps or I/O.
// Counts refer to requests (including duplicates), except unique_jobs/suppressed_jobs.
// Correlation is by request index within this batch only. Not a GPU capture.
inline std::string batch_diagnostics_json(const BatchResult& result,std::size_t max_items=256){
    const auto status=[](BatchStatus s){switch(s){
        case BatchStatus::Invalid:return "invalid";case BatchStatus::BudgetExceeded:return "budget_exceeded";
        case BatchStatus::Completed:return "completed";case BatchStatus::CallbackException:return "callback_exception";
        case BatchStatus::Suppressed:return "suppressed";default:return "unknown";}};
    const auto path=[](BuildPath p){switch(p){
        case BuildPath::Invalid:return "invalid";case BuildPath::Cold:return "cold";case BuildPath::Hit:return "hit";
        case BuildPath::Rebuilt:return "rebuilt";case BuildPath::Failed:return "failed";default:return "unknown";}};
    const auto write=[](WriteStatus w){switch(w){
        case WriteStatus::NotAttempted:return "not_attempted";case WriteStatus::Unavailable:return "unavailable";
        case WriteStatus::Stored:return "stored";case WriteStatus::Rejected:return "rejected";default:return "unknown";}};
    std::size_t invalid=0,budget=0,completed=0,exception=0,suppressed=0,unknown=0,usable=0,coalesced=0;
    for(const auto& i:result.items){
        switch(i.status){case BatchStatus::Invalid:++invalid;break;case BatchStatus::BudgetExceeded:++budget;break;
        case BatchStatus::Completed:++completed;break;case BatchStatus::CallbackException:++exception;break;
        case BatchStatus::Suppressed:++suppressed;break;default:++unknown;break;}
        if(i.status==BatchStatus::Completed&&(i.build.path==BuildPath::Cold||i.build.path==BuildPath::Hit||i.build.path==BuildPath::Rebuilt))++usable;
        if(i.coalesced)++coalesced;
    }
    const std::size_t count=std::min(result.items.size(),std::min(max_items,std::size_t{4096}));
    std::string out="{\"schema\":1";
    auto number=[&](const char* name,std::size_t v){out+=",\"";out+=name;out+="\":";out+=std::to_string(v);};
    number("requests",result.items.size());number("unique_jobs",result.unique_jobs);number("waves",result.waves);
    number("peak_wave_jobs",result.peak_wave_jobs);number("peak_estimated_bytes",result.peak_estimated_bytes);
    number("suppressed_jobs",result.suppressed_jobs);number("invalid_requests",invalid);number("budget_requests",budget);
    number("completed_requests",completed);number("exception_requests",exception);number("suppressed_requests",suppressed);
    number("unknown_requests",unknown);number("usable_requests",usable);number("coalesced_requests",coalesced);
    number("omitted_items",result.items.size()-count);out+=",\"items\":[";
    for(std::size_t n=0;n<count;++n){const auto& i=result.items[n];if(n)out+=',';out+="{\"index\":"+std::to_string(n);
        out+=",\"status\":\"";out+=status(i.status);out+="\",\"path\":\"";out+=path(i.build.path);
        out+="\",\"write\":\"";out+=write(i.build.write);out+='"';number("attempts",i.build.attempts);
        out+=",\"cache_rejected\":";out+=i.build.cache_rejected?"true":"false";
        out+=",\"coalesced\":";out+=i.coalesced?"true":"false";
        out+=",\"failure_remembered\":";out+=i.failure_remembered?"true":"false";out+='}';
    }
    out+="]}";return out;
}
}
