#include "native_pipeline_build.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace scarlett::pipeline;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void keys(){
    auto a=compute_key({1},{2,3});
    check(!a.empty()&&a==compute_key({1},{2,3}),"deterministic key");
    check(a!=compute_key({1,2},{3})&&a!=compute_key({1},{2,3},1)&&a!=compute_key({1},{2,3},0,1),"key ambiguity");
    check(a!=compute_key({2},{2,3})&&a!=compute_key({1},{2,4}),"content identity");
    check(compute_key({}, {1}).empty()&&compute_key({1},{}).empty(),"empty fields");
    check(compute_key(Bytes(MaxKeyBytes-25,1),{2}).size()==MaxKeyBytes,"exact key bound");
    check(compute_key(Bytes(MaxKeyBytes-24,1),{2}).empty(),"oversize key");
}
void run(){
    keys();const auto key=compute_key({1,2},{3,4});const Bytes old{9,8,7},fresh{6,5,4};
    const auto dir=std::filesystem::temp_directory_path()/("nativex-build-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(dir),"temp directory");
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}} cleanup{dir};
    auto file=dir/"cache.bin";Cache cache({1,2,3});unsigned calls=0;
    auto r=create_cached(cache,key,[&](const Bytes* cached){++calls;check(!cached,"cold input");return Attempt{AttemptStatus::Created,old};});
    check(r.path==BuildPath::Cold&&r.attempts==1&&r.write==WriteStatus::Stored&&calls==1,"cold creation");
    check(cache.save(file)==Status::Ok,"save");Cache reopened({1,2,3});check(reopened.load(file)==Status::Ok,"load");
    r=create_cached(reopened,key,[&](const Bytes* cached){++calls;check(cached&&*cached==old,"warm input");return Attempt{AttemptStatus::Created,{}};});
    check(r.path==BuildPath::Hit&&r.attempts==1&&r.write==WriteStatus::NotAttempted&&calls==2,"warm creation");
    r=create_cached(reopened,key,[&](const Bytes* cached){++calls;return cached?Attempt{AttemptStatus::CacheRejected,{}}:Attempt{AttemptStatus::Created,fresh};});
    check(r.path==BuildPath::Rebuilt&&r.cache_rejected&&r.attempts==2&&r.write==WriteStatus::Stored&&*reopened.find(key)==fresh&&calls==4,"fallback refresh");
    r=create_cached(reopened,key,[&](const Bytes* cached){++calls;check(cached&&*cached==fresh,"refreshed blob");return Attempt{AttemptStatus::Failed,{}};});
    check(r.path==BuildPath::Failed&&r.attempts==1&&!r.cache_rejected&&*reopened.find(key)==fresh&&calls==5,"unrelated failure evicted or retried");
    r=create_cached(reopened,key,[&](const Bytes* cached){++calls;return cached?Attempt{AttemptStatus::CacheRejected,{}}:Attempt{AttemptStatus::Failed,{}};});
    check(r.path==BuildPath::Failed&&r.attempts==2&&r.cache_rejected&&!reopened.find(key)&&calls==7,"failed rebuild published");
    r=create_cached(reopened,key,[&](const Bytes* cached){++calls;check(!cached,"stale cache retained");return Attempt{AttemptStatus::Created,{}};});
    check(r.path==BuildPath::Cold&&r.write==WriteStatus::Unavailable&&!reopened.find(key),"no blob");
    Cache full({1},25);r=create_cached(full,key,[&](const Bytes*){return Attempt{AttemptStatus::Created,fresh};});
    check(r.path==BuildPath::Cold&&r.write==WriteStatus::Rejected&&full.size()==0,"capacity must not fail creation");
    r=create_cached(reopened,{},[&](const Bytes*){++calls;return Attempt{};});check(r.path==BuildPath::Invalid&&r.attempts==0&&calls==8,"invalid key called backend");
    check(create_cached(reopened,key,{}).path==BuildPath::Invalid,"empty backend");
    check(reopened.put(key,old)==Status::Ok,"exception setup");
    bool thrown=false;try{create_cached(reopened,key,[](const Bytes*)->Attempt{throw std::runtime_error("backend");});}catch(const std::runtime_error&){thrown=true;}
    check(thrown&&*reopened.find(key)==old,"exception altered cache");
    // Independent sequence: every incompatibility can produce at most one
    // uncached call, all successful cold builds publish the exact payload.
    unsigned attempts=0,rebuilt=0,hits=0,failed=0;
    for(unsigned i=0;i<4096;++i){auto k=compute_key({1},{static_cast<std::uint8_t>(i%32)});bool had=reopened.find(k)!=nullptr;unsigned actual=0;
        auto result=create_cached(reopened,k,[&](const Bytes* cached){++actual;if(cached){if(i%7==0)return Attempt{AttemptStatus::CacheRejected,{}};if(i%11==0)return Attempt{AttemptStatus::Failed,{}};return Attempt{AttemptStatus::Created,{}};}return Attempt{AttemptStatus::Created,{static_cast<std::uint8_t>(i)}};});
        const bool reject=had&&i%7==0;const bool fail=had&&!reject&&i%11==0;
        check(actual==(reject?2u:1u)&&result.attempts==actual&&result.cache_rejected==reject,"retry count");
        check(result.path==(reject?BuildPath::Rebuilt:fail?BuildPath::Failed:had?BuildPath::Hit:BuildPath::Cold),"route mismatch");
        if(!had||reject)check(reopened.find(k)&&*reopened.find(k)==Bytes{static_cast<std::uint8_t>(i)},"fresh payload");
        attempts+=actual;rebuilt+=reject?1u:0u;hits+=had&&!reject&&!fail?1u:0u;failed+=fail?1u:0u;
    }
    std::cout<<"{\"requests\":4096,\"backend_attempts\":"<<attempts<<",\"rebuilt\":"<<rebuilt<<",\"hits\":"<<hits<<",\"unrelated_failures\":"<<failed<<",\"errors\":0}\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
