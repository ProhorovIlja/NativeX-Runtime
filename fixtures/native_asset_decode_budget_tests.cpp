// Original source-generated RGBA8 pages; CPU pixel assembly, not GPU rendering.
#include "native_asset_decode_budget.hpp"
#include "native_asset_singleflight.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <zstd.h>
using namespace scarlett;
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
stream::Result encode(const std::vector<std::uint8_t>& raw){
    std::vector<std::uint8_t> bytes(ZSTD_compressBound(raw.size()));
    const auto n=ZSTD_compress(bytes.data(),bytes.size(),raw.data(),raw.size(),3);
    require(!ZSTD_isError(n),"compression failed");bytes.resize(n);
    const auto hash=stream::fnv1a64(bytes);return {stream::Status::Ok,std::move(bytes),hash};
}
void controls(){
    using namespace assetdecode;
    auto input=encode(std::vector<std::uint8_t>(4096,17));
    BudgetedDecoder decoder(4096,2);
    auto held=decoder.decode(input,4096);
    require(held->status==Status::Ready&&held->bytes.size()==4096,"first decode");
    auto alias=held;
    require(decoder.stats().committed_bytes==4096&&decoder.stats().active==0,"live result accounting");
    auto rejected=decoder.decode(input,4096);
    require(rejected->status==Status::OutputBudgetExceeded&&rejected->bytes.empty(),"live cap bypassed");
    held.reset();require(decoder.stats().committed_bytes==4096,"early credit release");
    alias.reset();require(decoder.stats().committed_bytes==0,"last release failed");
    auto bad=input;bad.checksum^=1;
    require(decoder.decode(bad,4096)->status==Status::InputChecksumMismatch,"invalid input accepted");
    require(decoder.stats().committed_bytes==0&&decoder.stats().active==0,"error retained credit");
    require(decoder.decode(input,4095)->status==Status::SizeMismatch,"size mismatch");
    auto token=std::make_shared<std::atomic_bool>(true);
    require(decoder.decode(input,4096,{},token)->status==Status::Cancelled,"cancel ignored");
    require(decoder.stats().committed_bytes==0,"cancel retained credit");
    std::shared_ptr<const Decoded> survivor;
    {BudgetedDecoder short_lived(4096);survivor=short_lived.decode(input,4096);}
    require(survivor->status==Status::Ready&&survivor->bytes.front()==17,"lifetime failure");
    survivor.reset();
    bool invalid=false;try{BudgetedDecoder bad_limit(0);}catch(const std::invalid_argument&){invalid=true;}
    require(invalid,"invalid limit accepted");
}
void integration(){
    constexpr std::size_t width=128,height=64,page=width*height*4;
    std::vector<std::uint8_t> top(page),bottom(page);
    for(std::size_t i=0;i<page;i+=4){top[i]=255;top[i+3]=255;bottom[i+1]=255;bottom[i+3]=255;}
    auto a=encode(top),b=encode(bottom);
    const auto dir=std::filesystem::temp_directory_path()/("nativex-decode-pipeline-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(std::filesystem::create_directory(dir),"temp collision");
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove_all(path,e);}} cleanup{dir};
    auto path=dir/"pages.bin";
    {std::ofstream f(path,std::ios::binary);for(auto* x:{&a,&b})f.write(reinterpret_cast<const char*>(x->bytes.data()),static_cast<std::streamsize>(x->bytes.size()));require(bool(f),"write fixture");}
    stream::Reader source(2,8,4096);
    stream::CoalescedReader reader(source,4096,4,4,4096);
    assetdecode::BudgetedDecoder decoder(page*2,2);
    auto first=reader.read({"pages",1,0,a.bytes.size()},path,stream::Priority::FrameCritical);
    auto second=reader.read({"pages",1,a.bytes.size(),b.bytes.size()},path);
    auto x=decoder.decode(*first,page),y=decoder.decode(*second,page);
    require(x->status==assetdecode::Status::Ready&&y->status==assetdecode::Status::Ready,"read to decode failure");
    require(x->bytes==top&&y->bytes==bottom,"decoded page mismatch");
    std::vector<std::uint8_t> image;image.reserve(page*2);
    image.insert(image.end(),x->bytes.begin(),x->bytes.end());image.insert(image.end(),y->bytes.begin(),y->bytes.end());
    for(std::size_t i=0;i<image.size();i+=4){const bool is_top=i<page;require(image[i]==(is_top?255:0)&&image[i+1]==(is_top?0:255)&&image[i+2]==0&&image[i+3]==255,"assembled pixel mismatch");}
    require(decoder.stats().committed_bytes==page*2,"decoded pages not charged");
    require(reader.read({"pages",1,0,a.bytes.size()},path)==first&&reader.stats().leader_reads==2,"compressed cache reuse");
    x.reset();y.reset();require(decoder.stats().committed_bytes==0,"page credit leak");
    auto missing=reader.read({"pages",1,a.bytes.size()+b.bytes.size(),1},path);
    auto failed=decoder.decode(*missing,page);
    require(failed->status==assetdecode::Status::ReadFailed&&failed->bytes.empty(),"missing record exposed pixels");
    require(decoder.stats().committed_bytes==0,"failed record retained credit");
    std::cout<<"{\"verified_cpu_pixels\":"<<image.size()/4<<",\"reader_submissions\":2,\"peak_decoded_bytes\":"<<decoder.stats().peak_bytes<<"}\n";
}
void concurrent(){
    constexpr std::size_t size=65536;
    auto in=encode(std::vector<std::uint8_t>(size,91));
    assetdecode::BudgetedDecoder decoder(size*3,2);
    std::atomic<unsigned> errors{0},ok{0},denied{0};std::vector<std::thread> threads;
    for(unsigned t=0;t<8;++t)threads.emplace_back([&]{for(unsigned i=0;i<128;++i){
        auto r=decoder.decode(in,size);
        if(r->status==assetdecode::Status::Ready&&r->bytes.size()==size&&std::all_of(r->bytes.begin(),r->bytes.end(),[](std::uint8_t b){return b==91;}))++ok;
        else if(r->status==assetdecode::Status::OutputBudgetExceeded&&r->bytes.empty())++denied;
        else ++errors;
        auto s=decoder.stats();if(s.committed_bytes>s.byte_limit||s.peak_bytes>s.byte_limit||s.active>s.active_limit||s.peak_active>s.active_limit)++errors;
    }});
    for(auto& t:threads)t.join();
    auto s=decoder.stats();
    require(errors==0&&ok>0&&ok+denied==1024&&s.committed_bytes==0&&s.active==0,"concurrent budget invariant");
    std::cout<<"{\"attempts\":1024,\"accepted\":"<<ok<<",\"denied\":"<<denied<<",\"peak_bytes\":"<<s.peak_bytes<<",\"peak_active\":"<<s.peak_active<<",\"errors\":"<<errors<<"}\n";
}
}
int main(){try{controls();integration();concurrent();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
