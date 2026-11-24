#include "native_texture_upload.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace scarlett::upload;
namespace {
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
void run(){
    Description d{Format::Rgba8,65,3,3,2};
    auto p=plan(d);
    require(p.status==Status::Ready&&p.subresources.size()==6,"valid plan");
    require(p.subresources[0].row_bytes==260&&p.subresources[0].row_pitch==512&&p.subresources[0].rows==3,"RGBA footprint");
    require(p.subresources[1].offset==1536&&p.subresources[1].width==32&&p.subresources[1].height==1,"first mip");
    require(p.subresources[2].offset==2048&&p.subresources[3].offset==2560&&p.subresources[3].layer==1,"layer offsets");
    std::vector<std::vector<std::uint8_t>> src(p.subresources.size());std::vector<View> views;
    for(std::size_t i=0;i<src.size();++i){const auto& f=p.subresources[i];auto pitch=f.row_bytes+7;src[i].resize(pitch*(f.rows-1)+f.row_bytes,0xde);for(std::size_t r=0;r<f.rows;++r)for(std::size_t c=0;c<f.row_bytes;++c)src[i][r*pitch+c]=static_cast<std::uint8_t>((i*37+r*13+c)%251);views.push_back({src[i].data(),src[i].size(),pitch});}
    auto out=stage(d,views,p.bytes);require(out.plan.status==Status::Ready&&out.bytes.size()==p.bytes,"staging");
    std::vector<std::uint8_t> oracle(p.bytes,0);
    for(std::size_t i=0;i<views.size();++i){const auto& f=p.subresources[i];for(std::size_t r=0;r<f.rows;++r)for(std::size_t c=0;c<f.row_bytes;++c)oracle[f.offset+r*f.row_pitch+c]=static_cast<std::uint8_t>((i*37+r*13+c)%251);}
    require(out.bytes==oracle,"data or padding mismatch");
    require(plan(d,p.bytes-1).status==Status::BudgetExceeded,"budget");
    auto bad=views;--bad.back().size;auto failed=stage(d,bad);require(failed.plan.status==Status::InvalidSource&&failed.bytes.empty()&&failed.plan.subresources.empty(),"truncated publication");
    bad=views;bad[0].pitch=std::numeric_limits<std::size_t>::max();require(stage(d,bad).plan.status==Status::InvalidSource,"overflow source pitch");
    bad=views;bad[0].data=nullptr;require(stage(d,bad).plan.status==Status::InvalidSource,"null source");
    bad=views;bad.pop_back();require(stage(d,bad).plan.status==Status::InvalidSource,"missing subresource");
    for(auto format:{Format::Bc1,Format::Bc3,Format::Bc4,Format::Bc5,Format::Bc7}){
      auto bc=plan({format,8,8,4,1});auto unit=(format==Format::Bc1||format==Format::Bc4)?8u:16u;
      require(bc.status==Status::Ready&&bc.subresources[0].row_bytes==2*unit&&bc.subresources[0].rows==2,"BC top level");
      for(std::size_t i=1;i<4;++i)require(bc.subresources[i].row_bytes==unit&&bc.subresources[i].rows==1&&bc.subresources[i].offset==i*512,"BC small mip");
      std::vector<std::vector<std::uint8_t>> blocks;blocks.reserve(4);std::vector<View> v;
      for(const auto& f:bc.subresources){blocks.emplace_back(f.row_bytes*f.rows,0xab);v.push_back({blocks.back().data(),blocks.back().size(),f.row_bytes});}
      auto actual=stage({format,8,8,4,1},v);require(actual.plan.status==Status::Ready,"BC staging");
      for(const auto& f:bc.subresources)for(std::size_t r=0;r<f.rows;++r)for(std::size_t c=0;c<f.row_pitch;++c)require(actual.bytes[f.offset+r*f.row_pitch+c]==(c<f.row_bytes?0xab:0),"BC row data");
    }
    require(plan({Format::Rgba8,0,2,1,1}).status==Status::InvalidDescription,"zero dimension");
    require(plan({Format::Rgba8,2,2,3,1}).status==Status::InvalidDescription,"excess mips");
    require(plan({static_cast<Format>(99),4,4,1,1}).status==Status::InvalidDescription,"bad format");
    require(plan({Format::Rgba8,8192,8192,14,256}).status==Status::InvalidDescription,"subresource bound");
    require(plan({Format::Rgba8,8192,8192,1,1},1024).status==Status::BudgetExceeded,"large budget denial");
    std::cout<<"staging: six RGBA subresources, five BC formats, exact bytes and padding, invalid input controls PASS\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
