// Original Windows-only synthetic resource/descriptor lifetime integration.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include "native_gpu_lifetime.hpp"
using Microsoft::WRL::ComPtr;
using namespace scarlett::gpu_lifetime;
namespace {
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void hr(HRESULT result,const char* why){if(FAILED(result)){std::cerr<<why<<" HRESULT="<<std::hex<<static_cast<unsigned long>(result)<<'\n';throw std::runtime_error(why);}}
[[noreturn]] void pending_failure(){std::cerr<<"Uncertain GPU completion; terminating fixture\n";std::cerr.flush();ExitProcess(3);}
struct Engine {
    ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12Fence> fence;D3D12_COMMAND_LIST_TYPE type;
    Engine(ID3D12Device* d,D3D12_COMMAND_LIST_TYPE t):type(t){D3D12_COMMAND_QUEUE_DESC q{};q.Type=t;hr(d->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"queue");hr(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");}
    void execute(ID3D12GraphicsCommandList* list,Ledger& ledger,Queue q,Id value){
        HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);check(event!=nullptr,"event");
        ID3D12CommandList* lists[]={list};queue->ExecuteCommandLists(1,lists);
        if(FAILED(queue->Signal(fence.Get(),value))||!ledger.submit(q,value)||FAILED(fence->SetEventOnCompletion(value,event))||WaitForSingleObject(event,15000)!=WAIT_OBJECT_0)pending_failure();
        const Id observed=fence->GetCompletedValue();CloseHandle(event);if(observed!=value)pending_failure();
        // Observation is deliberately left to the coordinator.
    }
};
struct Commands {
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
    Commands(ID3D12Device* d,D3D12_COMMAND_LIST_TYPE t,ID3D12PipelineState* p=nullptr){hr(d->CreateCommandAllocator(t,IID_PPV_ARGS(&allocator)),"allocator");hr(d->CreateCommandList(0,t,allocator.Get(),p,IID_PPV_ARGS(&list)),"command list");}
};
D3D12_RESOURCE_DESC desc(D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=256;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;return d;}
ComPtr<ID3D12Resource> buffer(ID3D12Device* d,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE){D3D12_HEAP_PROPERTIES h{};h.Type=type;h.CreationNodeMask=1;h.VisibleNodeMask=1;auto rd=desc(flags);ComPtr<ID3D12Resource> r;hr(d->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&rd,state,nullptr,IID_PPV_ARGS(&r)),"buffer");return r;}
void transition(ID3D12GraphicsCommandList* list,ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=r;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=from;b.Transition.StateAfter=to;list->ResourceBarrier(1,&b);}
struct Pipeline {
    ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pso;
    explicit Pipeline(ID3D12Device* d){D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=1;
        D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable.NumDescriptorRanges=1;params[0].DescriptorTable.pDescriptorRanges=&range;params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
        D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=2;rd.pParameters=params;ComPtr<ID3DBlob> blob,err;hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&err),"root serialize");hr(d->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
        const char source[]="StructuredBuffer<uint> src:register(t0); RWStructuredBuffer<uint> dst:register(u0); [numthreads(64,1,1)] void main(uint3 i:SV_DispatchThreadID){dst[i.x]=src[i.x]^0x6a09e667u;}";
        ComPtr<ID3DBlob> shader;hr(D3DCompile(source,sizeof(source)-1,nullptr,nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_WARNINGS_ARE_ERRORS,0,&shader,&err),"compile original shader");D3D12_COMPUTE_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.CS={shader->GetBufferPointer(),shader->GetBufferSize()};hr(d->CreateComputePipelineState(&p,IID_PPV_ARGS(&pso)),"PSO");}
};
struct Generation {Record record;ComPtr<ID3D12Resource> resource;};
Generation upload(ID3D12Device* d,Engine& copy,Ledger& ledger,ID3D12DescriptorHeap* heap,Id bytes,Id fence,UINT seed){
    auto record=ledger.reserve(1,bytes,1).value();auto resource=buffer(d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);auto staging=buffer(d,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
    UINT values[64];for(UINT i=0;i<64;++i)values[i]=seed+i;void* ptr=nullptr;const D3D12_RANGE none{0,0};hr(staging->Map(0,&none,&ptr),"upload map");std::memcpy(ptr,values,sizeof(values));const D3D12_RANGE written{0,sizeof(values)};staging->Unmap(0,&written);
    auto cpu=heap->GetCPUDescriptorHandleForHeapStart();cpu.ptr+=record.offset*d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_UNKNOWN;srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Buffer.NumElements=64;srv.Buffer.StructureByteStride=4;d->CreateShaderResourceView(resource.Get(),&srv,cpu);
    Commands cmd(d,copy.type);cmd.list->CopyBufferRegion(resource.Get(),0,staging.Get(),0,256);hr(cmd.list->Close(),"copy close");check(ledger.begin(Queue::Copy,fence)&&ledger.upload(record.token,Queue::Copy),"register upload");check(!ledger.publish(record.token)&&!ledger.observe(Queue::Copy,fence),"early upload accepted");copy.execute(cmd.list.Get(),ledger,Queue::Copy,fence);check(ledger.observe(Queue::Copy,copy.fence->GetCompletedValue())&&ledger.publish(record.token),"publish completed upload");return {record,resource};
}
struct Read {
    ComPtr<ID3D12Resource> output,readback;Commands cmd;
    Read(ID3D12Device* d,Engine& engine,Pipeline& p,ID3D12DescriptorHeap* heap,const Generation& g):output(buffer(d,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)),readback(buffer(d,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST)),cmd(d,engine.type,p.pso.Get()){
        auto gpu=heap->GetGPUDescriptorHandleForHeapStart();gpu.ptr+=g.record.offset*static_cast<UINT64>(d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));ID3D12DescriptorHeap* heaps[]={heap};cmd.list->SetDescriptorHeaps(1,heaps);cmd.list->SetComputeRootSignature(p.root.Get());cmd.list->SetComputeRootDescriptorTable(0,gpu);cmd.list->SetComputeRootUnorderedAccessView(1,output->GetGPUVirtualAddress());cmd.list->Dispatch(1,1,1);transition(cmd.list.Get(),output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);cmd.list->CopyBufferRegion(readback.Get(),0,output.Get(),0,256);hr(cmd.list->Close(),"read close");}
    void verify(UINT seed){const D3D12_RANGE range{0,256},none{0,0};void* ptr=nullptr;hr(readback->Map(0,&range,&ptr),"readback map");auto data=static_cast<const UINT*>(ptr);bool ok=true;for(UINT i=0;i<64;++i)if(data[i]!=((seed+i)^0x6a09e667u))ok=false;readback->Unmap(0,&none);check(ok,"GPU readback mismatch");}
};
}
int main(int argc,char** argv){try{
    check(argc==2,"usage: d3d12_native_lifetime warp|nvidia");const std::string mode=argv[1];check(mode=="warp"||mode=="nvidia","adapter mode");ComPtr<IDXGIFactory4> factory;hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"factory");ComPtr<IDXGIAdapter1> adapter;
    if(mode=="warp")hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"WARP");else{for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;auto h=factory->EnumAdapters1(i,&a);if(h==DXGI_ERROR_NOT_FOUND)break;hr(h,"enumerate");DXGI_ADAPTER_DESC1 info{};hr(a->GetDesc1(&info),"adapter");if(info.VendorId==0x10de&&!(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){check(!adapter,"ambiguous NVIDIA adapter");adapter=a;}}check(bool(adapter),"NVIDIA not found");}
    DXGI_ADAPTER_DESC1 info{};hr(adapter->GetDesc1(&info),"adapter info");check((mode=="warp")==bool(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE),"adapter class");ComPtr<ID3D12Device> d;hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d)),"device");auto rd=desc();const auto alloc=d->GetResourceAllocationInfo(0,1,&rd);check(alloc.SizeInBytes>0&&alloc.SizeInBytes<=64ull*1024*1024,"allocation size");Ledger ledger(alloc.SizeInBytes*2,2);
    Engine copy(d.Get(),D3D12_COMMAND_LIST_TYPE_COPY),graphics(d.Get(),D3D12_COMMAND_LIST_TYPE_DIRECT),compute(d.Get(),D3D12_COMMAND_LIST_TYPE_COMPUTE);Pipeline p(d.Get());D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;ComPtr<ID3D12DescriptorHeap> heap;hr(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"descriptor heap");
    auto old=upload(d.Get(),copy,ledger,heap.Get(),alloc.SizeInBytes,1,100);check(ledger.begin(Queue::Graphics,1)&&ledger.begin(Queue::Compute,1)&&ledger.use(old.record.token,Queue::Graphics)&&ledger.use(old.record.token,Queue::Compute),"record old uses");Read a(d.Get(),graphics,p,heap.Get(),old),b(d.Get(),compute,p,heap.Get(),old);
    auto replacement=upload(d.Get(),copy,ledger,heap.Get(),alloc.SizeInBytes,2,200);check(ledger.active(1)==replacement.record.token&&old.record.offset!=replacement.record.offset&&!ledger.can_release(old.record.token)&&!ledger.reserve(2,1,1),"replacement protection");
    graphics.execute(a.cmd.list.Get(),ledger,Queue::Graphics,1);a.verify(100);check(ledger.observe(Queue::Graphics,graphics.fence->GetCompletedValue())&&!ledger.can_release(old.record.token),"compute protection");compute.execute(b.cmd.list.Get(),ledger,Queue::Compute,1);b.verify(100);check(ledger.observe(Queue::Compute,compute.fence->GetCompletedValue())&&ledger.can_release(old.record.token),"old release readiness");old.resource.Reset();check(ledger.released(old.record.token),"release old");
    auto recycled=ledger.reserve(2,alloc.SizeInBytes,1).value();check(recycled.offset==old.record.offset&&recycled.token!=old.record.token,"recycle range");check(ledger.retire(recycled.token)&&ledger.released(recycled.token),"retire unused reservation");
    check(ledger.begin(Queue::Compute,2)&&ledger.use(replacement.record.token,Queue::Compute),"record replacement");Read c(d.Get(),compute,p,heap.Get(),replacement);compute.execute(c.cmd.list.Get(),ledger,Queue::Compute,2);c.verify(200);check(ledger.observe(Queue::Compute,compute.fence->GetCompletedValue())&&ledger.retire(replacement.record.token)&&ledger.can_release(replacement.record.token),"replacement retirement");replacement.resource.Reset();check(ledger.released(replacement.record.token)&&ledger.stats().charged==0&&ledger.stats().descriptors==0,"zero ledger");hr(d->GetDeviceRemovedReason(),"device status");std::cout<<"adapter="<<mode<<" vendor="<<info.VendorId<<" device="<<info.DeviceId<<" software="<<bool(info.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)<<" queues=3 compared_words=192 allocation_bytes="<<alloc.SizeInBytes<<" peak_charged="<<ledger.stats().peak<<" errors=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
