// Independent fixture: reserved critical read admission, no external assets.
#include "native_asset_stream.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace scarlett::stream;
namespace {
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
struct Temp {
    std::filesystem::path path;
    Temp(){path=std::filesystem::temp_directory_path()/("nativex-reserve-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));require(std::filesystem::create_directory(path),"temporary directory collision");}
    ~Temp(){std::error_code ec;std::filesystem::remove_all(path,ec);}
};
void run(){
    Temp temp;
    const auto path=temp.path/"fixture.bin";
    constexpr std::size_t chunk=64u*1024u;
    std::vector<std::uint8_t> data(4*chunk);
    for(std::size_t i=0;i<data.size();++i)data[i]=static_cast<std::uint8_t>((i*13u+7u)%251u);
    {std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()));require(bool(out),"fixture write");}
    for(auto r:{AdmissionReserve{4*chunk+1,0},AdmissionReserve{0,9}}){
        bool rejected=false;try{Reader invalid(1,8,4*chunk,r);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"invalid reserve accepted");
    }
    Reader reader(2,8,4*chunk,{2*chunk,2});
    for(auto p:{Priority::Normal,Priority::Background}){
        require(reader.submit({path,0,3*chunk,{},p}).get().status==Status::BudgetExceeded,"noncritical exceeded byte share");
        require(reader.submit({path,0,2*chunk,{},p}).get().status==Status::Ok,"exact noncritical byte share rejected");
    }
    require(reader.submit({path,0,4*chunk,{},Priority::FrameCritical}).get().status==Status::Ok,"critical could not use full budget");
    require(reader.submit({temp.path/"missing",0,chunk,{},Priority::Normal}).get().status==Status::FileError,"file error lost");
    require(reader.stats().noncritical_outstanding_bytes==0,"error retained noncritical credit");
    auto token=std::make_shared<std::atomic_bool>(true);
    require(reader.submit({path,0,chunk,token,Priority::Background}).get().status==Status::Cancelled,"cancel status lost");
    require(reader.stats().noncritical_outstanding_bytes==0,"cancel retained noncritical credit");
    {Reader slots(1,2,4*chunk,{0,2});
      require(slots.submit({path,0,0,{},Priority::Normal}).get().status==Status::QueueFull,"reserved queue accepted normal even empty");
      require(slots.submit({path,0,chunk,{},Priority::FrameCritical}).get().status==Status::Ok,"reserved slot inaccessible");}
    {Reader bytes(1,2,4*chunk,{4*chunk,0});
      require(bytes.submit({path,0,1,{},Priority::Background}).get().status==Status::BudgetExceeded,"full byte reserve bypassed");
      require(bytes.submit({path,data.size(),0,{},Priority::Background}).get().status==Status::Ok,"zero byte request failed");}
    {Reader legacy(1,2,4*chunk);require(legacy.submit({path,0,4*chunk,{}}).get().status==Status::Ok,"default compatibility");}
    std::atomic<unsigned> ok{0},denied{0},errors{0};
    std::array<std::atomic<unsigned>,3> by_lane{};
    for(auto& count:by_lane)count.store(0);
    std::vector<std::thread> producers;
    for(unsigned lane=0;lane<12;++lane)producers.emplace_back([&,lane]{
        for(unsigned i=0;i<128;++i){
            auto offset=(i%4)*chunk;
            auto result=reader.submit({path,offset,chunk,{},static_cast<Priority>(lane%3)}).get();
            if(result.status==Status::QueueFull||result.status==Status::BudgetExceeded)++denied;
            else if(result.status==Status::Ok&&result.bytes.size()==chunk&&std::equal(result.bytes.begin(),result.bytes.end(),data.begin()+static_cast<std::ptrdiff_t>(offset))&&result.checksum==fnv1a64(result.bytes)){++ok;++by_lane[lane%3];}
            else ++errors;
            auto snap=reader.stats();
            if(snap.outstanding_bytes>snap.byte_budget||snap.noncritical_outstanding_bytes>snap.byte_budget-snap.reserve.bytes||snap.noncritical_outstanding_bytes>snap.outstanding_bytes||snap.noncritical_queued>snap.queue_limit-snap.reserve.queued_slots||snap.queued>snap.queue_limit||snap.active>2)++errors;
        }
    });
    for(auto& t:producers)t.join();
    auto snap=reader.stats();
    require(errors==0&&ok+denied==1536&&ok>0,"concurrent admission invariant");
    require(snap.active==0&&snap.queued==0&&snap.outstanding_bytes==0&&snap.noncritical_outstanding_bytes==0,"credit leak after concurrent requests");
    require(snap.peak_outstanding_bytes<=snap.byte_budget,"global peak exceeds cap");
    require(reader.submit({path,0,2*chunk,{}}).get().status==Status::Ok,"noncritical credit not reusable");
    std::cout<<"{\"attempts\":1536,\"completed\":"<<ok<<",\"denied\":"<<denied<<",\"critical_completed\":"<<by_lane[0]<<",\"normal_completed\":"<<by_lane[1]<<",\"background_completed\":"<<by_lane[2]<<",\"peak_bytes\":"<<snap.peak_outstanding_bytes<<",\"budget_bytes\":"<<snap.byte_budget<<",\"reserved_bytes\":"<<snap.reserve.bytes<<",\"reserved_slots\":"<<snap.reserve.queued_slots<<",\"invariant_errors\":"<<errors<<"}\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
