// Original Windows fixture. Synthetic textures only; no game assets.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "native_texture_upload.hpp"
#include "native_upload_arena.hpp"
#include <array>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
using namespace scarlett::upload;
namespace {
void check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
void hrcheck(HRESULT hr,const char* what){if(FAILED(hr)){std::cerr<<what<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';throw std::runtime_error(what);}}
// After submission, uncertain completion must not release/reuse resources
// while GPU work may remain. This is a standalone test process, not a runtime.
[[noreturn]] void abort_inflight(const char* what){std::cerr<<what<<'\n';std::cerr.flush();TerminateProcess(GetCurrentProcess(),1);std::abort();}
D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES h{};h.Type=type;h.CreationNodeMask=1;h.VisibleNodeMask=1;return h;}
D3D12_RESOURCE_DESC buffer(UINT64 size){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=size;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;return d;}
ComPtr<ID3D12Resource> resource(ID3D12Device* dev,D3D12_HEAP_TYPE type,const D3D12_RESOURCE_DESC& d,D3D12_RESOURCE_STATES state){ComPtr<ID3D12Resource> r;const auto h=heap(type);hrcheck(dev->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)),"create resource");return r;}
D3D12_TEXTURE_COPY_LOCATION placed(ID3D12Resource* r,D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp,UINT64 base){D3D12_TEXTURE_COPY_LOCATION l{};l.pResource=r;l.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;l.PlacedFootprint=fp;l.PlacedFootprint.Offset+=base;return l;}
D3D12_TEXTURE_COPY_LOCATION subresource(ID3D12Resource* r,UINT i){D3D12_TEXTURE_COPY_LOCATION l{};l.pResource=r;l.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;l.SubresourceIndex=i;return l;}
struct Event{HANDLE value=CreateEventW(nullptr,FALSE,FALSE,nullptr);~Event(){if(value)CloseHandle(value);}};
void wait_for(ID3D12Fence* fence,UINT64 value,HANDLE event){
    auto completed=fence->GetCompletedValue();
    if(completed==UINT64_MAX)abort_inflight("device removed before wait");
    if(completed<value){if(FAILED(fence->SetEventOnCompletion(value,event))||WaitForSingleObject(event,10000)!=WAIT_OBJECT_0)abort_inflight("GPU fence wait failed");}
    completed=fence->GetCompletedValue();if(completed==UINT64_MAX||completed<value)abort_inflight("GPU fence completion invalid");
}
struct Job{ComPtr<ID3D12Resource> texture,readback;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> commands;Staged staged;std::vector<std::vector<std::uint8_t>> source;Slice slice;};
int run(bool hardware){
    ComPtr<IDXGIFactory4> factory;hrcheck(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"DXGI factory");
    ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;
    if(hardware){for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> candidate;auto hr=factory->EnumAdapters1(i,&candidate);if(hr==DXGI_ERROR_NOT_FOUND)break;hrcheck(hr,"enumerate adapter");DXGI_ADAPTER_DESC1 desc{};hrcheck(candidate->GetDesc1(&desc),"adapter desc");if((desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)||desc.VendorId!=0x10de)continue;if(SUCCEEDED(D3D12CreateDevice(candidate.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)))){adapter=candidate;break;}}}
    else {if(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));}
    if(!device){std::cout<<"SKIP: requested adapter unavailable\n";return 77;}
    DXGI_ADAPTER_DESC1 ad{};hrcheck(adapter->GetDesc1(&ad),"adapter desc");std::wcerr<<L"adapter: "<<ad.Description<<L'\n';
    constexpr UINT count=6;const Description desc{Format::Rgba8,65,3,3,2};const auto p=plan(desc);check(p.status==Status::Ready&&p.subresources.size()==count,"CPU plan");
    D3D12_RESOURCE_DESC td{};td.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;td.Width=desc.width;td.Height=desc.height;td.DepthOrArraySize=static_cast<UINT16>(desc.layers);td.MipLevels=static_cast<UINT16>(desc.mips);td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.SampleDesc.Count=1;td.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    // Compare geometry and placement against the actual device before any copy.
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT,count> fps{};std::array<UINT,count> rows{};std::array<UINT64,count> rowbytes{};UINT64 total=0;
    device->GetCopyableFootprints(&td,0,count,0,fps.data(),rows.data(),rowbytes.data(),&total);
    check(total>0&&total<=p.bytes,"device footprint extent");
    for(UINT i=0;i<count;++i){const auto& a=p.subresources[i];const auto& b=fps[i];check(a.offset==b.Offset&&a.row_pitch==b.Footprint.RowPitch&&a.rows==rows[i]&&a.row_bytes==rowbytes[i]&&a.width==b.Footprint.Width&&a.height==b.Footprint.Height&&b.Footprint.Depth==1&&b.Footprint.Format==td.Format,"device footprint differs from CPU plan");}
    // Capacity rounded to 512 for two independently aligned slices.
    const auto stride=(p.bytes+511u)&~std::size_t{511};UploadArena arena(stride*2,2);
    auto upload=resource(device.Get(),D3D12_HEAP_TYPE_UPLOAD,buffer(stride*2),D3D12_RESOURCE_STATE_GENERIC_READ);
    D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> queue;hrcheck(device->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"queue");
    ComPtr<ID3D12Fence> fence;hrcheck(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");Event event;check(event.value!=nullptr,"event");
    const D3D12_RANGE no_read{0,0};void* raw=nullptr;hrcheck(upload->Map(0,&no_read,&raw),"map upload");auto* mapped=static_cast<std::uint8_t*>(raw);
    std::array<Job,2> jobs;
    for(std::size_t j=0;j<jobs.size();++j){auto& job=jobs[j];job.source.resize(count);std::vector<View> views;views.reserve(count);
        for(UINT i=0;i<count;++i){const auto& f=p.subresources[i];auto& data=job.source[i];data.resize(f.row_bytes*f.rows);for(std::size_t b=0;b<data.size();++b)data[b]=static_cast<std::uint8_t>((j*101u+i*29u+b*17u)%251u);views.push_back({data.data(),data.size(),f.row_bytes});}
        job.staged=stage(desc,views);check(job.staged.plan.status==Status::Ready,"stage");auto slice=arena.reserve(job.staged.bytes.size());check(bool(slice),"reserve");job.slice=*slice;std::memcpy(mapped+slice->offset,job.staged.bytes.data(),slice->bytes);
        job.texture=resource(device.Get(),D3D12_HEAP_TYPE_DEFAULT,td,D3D12_RESOURCE_STATE_COPY_DEST);job.readback=resource(device.Get(),D3D12_HEAP_TYPE_READBACK,buffer(p.bytes),D3D12_RESOURCE_STATE_COPY_DEST);
        hrcheck(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&job.allocator)),"allocator");hrcheck(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,job.allocator.Get(),nullptr,IID_PPV_ARGS(&job.commands)),"command list");
        for(UINT i=0;i<count;++i){auto src=placed(upload.Get(),fps[i],slice->offset);auto dst=subresource(job.texture.Get(),i);job.commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=job.texture.Get();barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;job.commands->ResourceBarrier(1,&barrier);
        for(UINT i=0;i<count;++i){auto src=subresource(job.texture.Get(),i);auto dst=placed(job.readback.Get(),fps[i],0);job.commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}hrcheck(job.commands->Close(),"close list");
    }
    check(arena.seal(jobs[0].slice,1)&&arena.seal(jobs[1].slice,2)&&!arena.reserve(1),"fence reservation");
    // Everything potentially allocating is prepared before submitting work.
    for(std::size_t j=0;j<jobs.size();++j){ID3D12CommandList* lists[]{jobs[j].commands.Get()};queue->ExecuteCommandLists(1,lists);if(FAILED(queue->Signal(fence.Get(),j+1)))abort_inflight("queue signal failed");}
    wait_for(fence.Get(),1,event.value);
    // Fence 1 is a proven lower bound even if fence 2 has already completed.
    try {check(arena.collect(1),"collect fence 1");auto reuse=arena.reserve(jobs[0].slice.bytes);check(reuse&&reuse->offset==jobs[0].slice.offset,"reuse first slice");std::memset(mapped+reuse->offset,0x7f,reuse->bytes);check(arena.cancel(*reuse),"cancel reuse");}catch(...){abort_inflight("allocator retirement failed");}
    wait_for(fence.Get(),2,event.value);hrcheck(device->GetDeviceRemovedReason(),"device status");check(arena.collect(2)&&arena.stats().used==0,"retire all");upload->Unmap(0,nullptr);
    std::size_t compared=0;for(auto& job:jobs){void* ptr=nullptr;const D3D12_RANGE range{0,p.bytes};hrcheck(job.readback->Map(0,&range,&ptr),"map readback");const auto* bytes=static_cast<const std::uint8_t*>(ptr);bool same=true;for(UINT i=0;i<count;++i){const auto& f=p.subresources[i];for(std::size_t r=0;r<f.rows;++r){if(std::memcmp(bytes+f.offset+r*f.row_pitch,job.source[i].data()+r*f.row_bytes,f.row_bytes)!=0)same=false;compared+=f.row_bytes;}}job.readback->Unmap(0,&no_read);check(same,"GPU readback mismatch");}
    std::cout<<"{\"subresources\":12,\"verified_bytes\":"<<compared<<",\"hardware\":"<<(hardware?"true":"false")<<",\"remaining_bytes\":0}\n";return 0;
}
}
int main(int argc,char** argv){try{if(argc!=2||(std::string(argv[1])!="warp"&&std::string(argv[1])!="nvidia")){std::cerr<<"usage: d3d12_native_upload_roundtrip warp|nvidia\n";return 2;}return run(std::string(argv[1])=="nvidia");}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
