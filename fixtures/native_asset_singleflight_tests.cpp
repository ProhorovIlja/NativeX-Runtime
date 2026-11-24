// Original test data and controlled backends; no external asset files.
#include "native_asset_singleflight.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>
using namespace scarlett::stream;
namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
Result bytes(std::size_t n,std::uint8_t value=17){Result r{Status::Ok,std::vector<std::uint8_t>(n,value),0};r.checksum=fnv1a64(r.bytes);return r;}
struct Gate {
    std::mutex mutex;
    std::condition_variable cv;
    bool open=false;
    unsigned entered=0;
    void wait(){std::unique_lock<std::mutex> lock(mutex);++entered;cv.notify_all();cv.wait(lock,[&]{return open;});}
    bool until(unsigned n){std::unique_lock<std::mutex> lock(mutex);return cv.wait_for(lock,std::chrono::seconds(3),[&]{return entered>=n;});}
    void release(){std::lock_guard<std::mutex> lock(mutex);open=true;cv.notify_all();}
};
template<typename Predicate> bool until(Predicate pred){const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(3);while(!pred()){if(std::chrono::steady_clock::now()>=end)return false;std::this_thread::yield();}return true;}
void group(){
    Gate gate;
    std::atomic<unsigned> calls{0};
    CoalescedReader reader([&](Request r){++calls;gate.wait();return bytes(r.length);},4096,16,2);
    CacheKey key{"asset",1,0,1024};
    auto token=std::make_shared<std::atomic_bool>(false);
    auto leader=std::async(std::launch::async,[&]{return reader.read(key,"fixture",Priority::Normal,token);});
    const bool started=gate.until(1);
    std::vector<std::future<std::shared_ptr<const Result>>> followers;
    for(int i=0;i<15;++i)followers.push_back(std::async(std::launch::async,[&]{return reader.read(key,"fixture");}));
    const bool joined=until([&]{return reader.stats().joined==15;});
    token->store(true);gate.release();
    require(started&&joined,"controlled group did not join");
    require(leader.get()->status==Status::Cancelled,"leader cancellation ignored");
    std::shared_ptr<const Result> held;
    for(auto& f:followers){auto r=f.get();require(r->status==Status::Ok&&r->bytes==bytes(1024).bytes,"follower result");if(held)require(held==r,"followers copied buffers");held=r;}
    require(calls==1&&reader.stats().active==0,"duplicate backend or leaked flight");
    require(reader.read(key,"fixture")==held,"cache did not reuse publication");
    require(reader.stats().cache_hits==1,"cache hit count");
    require(reader.read({"asset",2,0,1024},"fixture")->status==Status::Ok&&calls==2,"generation not distinct");
    std::cout<<"group: 16 callers, 1 backend read, 15 joins, cancelled leader isolated\n";
}
void priority_and_limit(){
    Gate gate;
    CoalescedReader r([&](Request req){gate.wait();return bytes(req.length);},4096,8,2);
    CacheKey key{"same",1,0,32};
    auto normal=std::async(std::launch::async,[&]{return r.read(key,"fixture");});
    const bool first=gate.until(1);
    auto critical=std::async(std::launch::async,[&]{return r.read(key,"fixture",Priority::FrameCritical);});
    const bool second=gate.until(2);
    const auto denied=r.read({"other",1,0,32},"fixture");
    gate.release();
    require(first&&second,"critical read demoted to normal flight");
    require(denied->status==Status::QueueFull,"flight limit not enforced");
    require(normal.get()==critical.get(),"same immutable result not shared after publication");
    require(r.stats().active==0&&r.stats().leader_reads==2&&r.stats().rejected==1,"flight accounting");
}
void failures(){
    unsigned calls=0;
    CoalescedReader r([&](Request req)->Result{++calls;if(calls==1)return {Status::FileError,{},0};if(calls==2)throw std::runtime_error("test");return bytes(req.length);},16,2,1);
    CacheKey key{"a",1,0,32};
    require(r.read(key,"fixture")->status==Status::FileError,"file error lost");
    bool thrown=false;try{r.read(key,"fixture");}catch(const std::runtime_error&){thrown=true;}
    require(thrown&&r.stats().active==0,"exception retained flight");
    require(r.read(key,"fixture")->status==Status::Ok,"retry failed");
    require(r.read(key,"fixture")->status==Status::Ok&&calls==4,"oversized result cached");
    require(r.cache_stats().entries==0,"oversized cache publication");
    auto pre=std::make_shared<std::atomic_bool>(true);
    require(r.read(key,"fixture",Priority::Normal,pre)->status==Status::Cancelled&&calls==4,"pre-cancel admitted");
    require(r.read({"",1,0,32},"fixture")->status==Status::InvalidRange&&calls==4,"invalid admitted");
    CoalescedReader bad([](Request req){auto r=bytes(req.length);r.checksum=0;return r;},32,1,1);
    thrown=false;try{bad.read(key,"fixture");}catch(const std::runtime_error&){thrown=true;}
    require(thrown&&bad.stats().active==0&&bad.cache_stats().entries==0,"invalid result published");
}
void exception_group(){
    Gate gate;
    CoalescedReader r([&](Request)->Result{gate.wait();throw std::runtime_error("shared failure");},1024);
    auto attempt=[&]{try{r.read({"a",1,0,16},"fixture");}catch(const std::runtime_error&){return true;}return false;};
    auto a=std::async(std::launch::async,attempt);const bool first=gate.until(1);
    auto b=std::async(std::launch::async,attempt);const bool joined=until([&]{return r.stats().joined==1;});gate.release();
    require(first&&joined&&a.get()&&b.get()&&r.stats().active==0,"shared exception not delivered");
}
void integrated(){
    const auto dir=std::filesystem::temp_directory_path()/("nativex-flight-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(std::filesystem::create_directory(dir),"temp collision");
    struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}} cleanup{dir};
    const auto path=dir/"fixture.bin";
    auto data=bytes(8192,93);
    {std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(data.bytes.data()),static_cast<std::streamsize>(data.bytes.size()));require(bool(f),"fixture write");}
    Reader backend(2,8,8192);
    CoalescedReader r(backend,8192,2,4);
    std::atomic<unsigned> errors{0};std::vector<std::thread> callers;
    for(int t=0;t<8;++t)callers.emplace_back([&]{for(int i=0;i<128;++i){auto v=r.read({"fixture",1,static_cast<std::uint64_t>(i%2)*4096,4096},path);if(v->status!=Status::Ok||v->bytes.size()!=4096||v->bytes.front()!=93||v->checksum!=fnv1a64(v->bytes))++errors;}});
    for(auto& t:callers)t.join();
    auto s=r.stats();
    require(errors==0&&s.leader_reads==2&&s.joined+s.cache_hits==1022&&s.active==0,"real reader coalescing/cache mismatch");
    std::cout<<"{\"logical_reads\":1024,\"backend_reads\":"<<s.leader_reads<<",\"joined\":"<<s.joined<<",\"cache_hits\":"<<s.cache_hits<<"}\n";
}
}
int main(){try{group();priority_and_limit();failures();exception_group();integrated();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
