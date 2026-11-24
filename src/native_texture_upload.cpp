#include "native_texture_upload.hpp"
#include <algorithm>
#include <cstring>
#include <utility>
namespace scarlett::upload {
namespace {
constexpr std::size_t RowAlignment=256, PlacementAlignment=512;
std::size_t aligned(std::size_t n,std::size_t a){return (n+a-1)/a*a;}
Plan failed(Status s){return {s,0,{}};}
}
Plan plan(Description d,std::size_t budget){
    std::size_t block=0,unit=4;
    switch(d.format){
      case Format::Rgba8:block=1;break;
      case Format::Bc1:case Format::Bc4:block=4;unit=8;break;
      case Format::Bc3:case Format::Bc5:case Format::Bc7:block=4;unit=16;break;
      default:return failed(Status::InvalidDescription);
    }
    if(!d.width||!d.height||d.width>8192||d.height>8192||!d.mips||!d.layers||d.layers>256||!budget||budget>MaxStagingBytes)
        return failed(Status::InvalidDescription);
    std::uint32_t max_mips=1;for(auto n=std::max(d.width,d.height);n>1;n/=2)++max_mips;
    if(d.mips>max_mips||d.layers*d.mips>2048)return failed(Status::InvalidDescription);
    Plan p{Status::Ready,0,{}};p.subresources.reserve(d.layers*d.mips);
    for(std::uint32_t layer=0;layer<d.layers;++layer){
        for(std::uint32_t mip=0;mip<d.mips;++mip){
            auto w=std::max(1u,d.width>>mip),h=std::max(1u,d.height>>mip);
            auto row=((static_cast<std::size_t>(w)+block-1)/block)*unit;
            auto rows=(static_cast<std::size_t>(h)+block-1)/block;
            auto offset=aligned(p.bytes,PlacementAlignment),pitch=aligned(row,RowAlignment);
            // Dimensions and prior extent are bounded; division avoids overflow.
            if(offset>budget||rows>(budget-offset)/pitch)return failed(Status::BudgetExceeded);
            p.subresources.push_back({mip,layer,w,h,offset,row,pitch,rows});
            p.bytes=offset+pitch*rows;
        }
    }
    return p;
}
Staged stage(Description d,const std::vector<View>& sources,std::size_t budget){
    auto p=plan(d,budget);
    if(p.status!=Status::Ready)return {std::move(p),{}};
    if(sources.size()!=p.subresources.size())return {failed(Status::InvalidSource),{}};
    for(std::size_t i=0;i<sources.size();++i){
        const auto& s=sources[i];const auto& f=p.subresources[i];
        if(!s.data||s.pitch<f.row_bytes||s.size<f.row_bytes||
           (f.rows-1)>(s.size-f.row_bytes)/s.pitch)return {failed(Status::InvalidSource),{}};
    }
    Staged out{std::move(p),{}};out.bytes.resize(out.plan.bytes,0);
    for(std::size_t i=0;i<sources.size();++i){
        const auto& s=sources[i];const auto& f=out.plan.subresources[i];
        for(std::size_t r=0;r<f.rows;++r)
            std::memcpy(out.bytes.data()+f.offset+r*f.row_pitch,s.data+r*s.pitch,f.row_bytes);
    }
    return out;
}
} // namespace scarlett::upload
