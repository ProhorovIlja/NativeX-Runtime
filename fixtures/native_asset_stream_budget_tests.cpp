// Independent native read admission tests; no external assets.
#include "native_asset_stream.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace scarlett::stream;
namespace {
void require(bool value, const char* reason) { if(!value) throw std::runtime_error(reason); }
struct Temp {
    std::filesystem::path dir;
    Temp() {
        dir=std::filesystem::temp_directory_path()/
            ("nativex-budget-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        require(std::filesystem::create_directory(dir),"temporary path collision");
    }
    ~Temp(){std::error_code ec;std::filesystem::remove_all(dir,ec);}
};
void run(){
    Temp temp;
    auto path=temp.dir/"source.bin";
    const std::size_t chunk=64u*1024u;
    std::vector<std::uint8_t> data(chunk*4u);
    for(std::size_t i=0;i<data.size();++i)data[i]=static_cast<std::uint8_t>((i*19u+7u)%251u);
    {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));require(bool(out),"fixture write");}
    Reader reader(2,64,chunk*2);
    require(reader.submit({path,0,chunk*3,{}}).get().status==Status::BudgetExceeded,"oversized budget admission");
    require(reader.stats().peak_outstanding_bytes==0,"rejected request consumed credit");
    auto cancelled=std::make_shared<std::atomic_bool>(true);
    require(reader.submit({path,0,chunk,cancelled}).get().status==Status::Cancelled,"pre-cancelled status");
    require(reader.stats().outstanding_bytes==0,"cancelled credit leaked");
    require(reader.submit({temp.dir/"missing",0,chunk,{}}).get().status==Status::FileError,"missing file status");
    require(reader.stats().outstanding_bytes==0,"error leaked credit");
    require(reader.submit({path,0,chunk*2,{}}).get().status==Status::Ok,"exact budget rejected");
    require(reader.stats().peak_outstanding_bytes==chunk*2,"peak accounting");
    require(reader.stats().outstanding_bytes==0&&reader.stats().active==0,"ready future retained credit");
    require(reader.submit({path,data.size(),0,{}}).get().status==Status::Ok,"zero length at EOF");
    std::atomic<unsigned> completed{0}, rejected{0}, errors{0};
    std::vector<std::thread> producers;
    const auto start=std::chrono::steady_clock::now();
    for(unsigned lane=0;lane<4;++lane)producers.emplace_back([&,lane]{
        for(unsigned i=0;i<128;++i){
            const auto offset=(i%4)*chunk;
            auto result=reader.submit({path,offset,chunk,{},static_cast<Priority>(lane%3)}).get();
            if(result.status==Status::BudgetExceeded||result.status==Status::QueueFull){++rejected;continue;}
            if(result.status!=Status::Ok||result.bytes.size()!=chunk||
               !std::equal(result.bytes.begin(),result.bytes.end(),data.begin()+static_cast<std::ptrdiff_t>(offset))||
               result.checksum!=fnv1a64(result.bytes)){++errors;continue;}
            ++completed;
            auto snap=reader.stats();
            if(snap.outstanding_bytes>snap.byte_budget||snap.peak_outstanding_bytes>snap.byte_budget||snap.active>2)++errors;
        }
    });
    for(auto& t:producers)t.join();
    auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    require(errors==0&&completed>0&&completed+rejected==512,"concurrent admission or data mismatch");
    const auto stats=reader.stats();
    require(stats.queued==0&&stats.active==0&&stats.outstanding_bytes==0,"credits not drained");
    require(stats.peak_outstanding_bytes<=stats.byte_budget,"budget exceeded");
    // Retained completed buffers are intentionally outside the reader budget.
    std::vector<Result> retained;
    for(int i=0;i<4;++i){retained.push_back(reader.submit({path,0,chunk,{}}).get());require(retained.back().status==Status::Ok,"credit not reusable");}
    require(reader.stats().outstanding_bytes==0,"completed buffers counted");
    bool bad=false;try{Reader invalid(1,1,0);}catch(const std::invalid_argument&){bad=true;}
    require(bad,"zero budget accepted");
    std::cout<<"{\"attempts\":512,\"completed\":"<<completed<<",\"rejected\":"<<rejected
             <<",\"peak_requested_bytes\":"<<stats.peak_outstanding_bytes<<",\"budget_bytes\":"<<stats.byte_budget
             <<",\"elapsed_ms\":"<<ms<<"}\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
