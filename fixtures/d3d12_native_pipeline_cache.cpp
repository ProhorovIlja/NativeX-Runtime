// Original local D3D12 compute cache fixture. No game input. Same device/run
// only: the temporary cache is NOT a persistent cross-driver identity probe.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "native_pipeline_build.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
using Microsoft::WRL::ComPtr;
using namespace scarlett::pipeline;
namespace {
using Clock=std::chrono::steady_clock;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void checked(HRESULT hr,const char* why){if(FAILED(hr)){std::cerr<<why<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';throw std::runtime_error(why);}}
[[noreturn]] void abort_pending(const char* why){std::cerr<<why<<'\n';std::cerr.flush();TerminateProcess(GetCurrentProcess(),1);std::abort();}
struct Event{HANDLE h=CreateEventW(nullptr,FALSE,FALSE,nullptr);~Event(){if(h)CloseHandle(h);}};
struct Temp{std::filesystem::path p;~Temp(){if(!p.empty()){std::error_code e;std::filesystem::remove_all(p,e);}}};
D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES p{};p.Type=type;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;}
D3D12_RESOURCE_DESC buffer(UINT64 n,D3D12_RESOURCE_FLAGS flags){D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=n;d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;return d;}
ComPtr<ID3D12Resource> resource(ID3D12Device* dev,D3D12_HEAP_TYPE type,const D3D12_RESOURCE_DESC& d,D3D12_RESOURCE_STATES state){ComPtr<ID3D12Resource> r;auto h=heap(type);checked(dev->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)),"resource");return r;}
Bytes blob_bytes(ID3DBlob* b){const auto* p=static_cast<const std::uint8_t*>(b->GetBufferPointer());return Bytes(p,p+b->GetBufferSize());}
// D3D12's documented cache-specific error codes; E_INVALIDARG is ambiguous,
// so allow one fresh attempt only when a cached blob was actually supplied.
bool cache_rejection(HRESULT hr){return hr==D3D12_ERROR_DRIVER_VERSION_MISMATCH||hr==D3D12_ERROR_ADAPTER_NOT_FOUND||hr==E_INVALIDARG;}
void dispatch_and_verify(ID3D12Device* dev,ID3D12RootSignature* root,ID3D12PipelineState* pso){
    constexpr UINT count=64;constexpr UINT64 bytes=count*sizeof(std::uint32_t);
    auto gpu=resource(dev,D3D12_HEAP_TYPE_DEFAULT,buffer(bytes,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    auto read=resource(dev,D3D12_HEAP_TYPE_READBACK,buffer(bytes,D3D12_RESOURCE_FLAG_NONE),D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_COMPUTE;
    ComPtr<ID3D12CommandQueue> queue;checked(dev->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"queue");
    ComPtr<ID3D12CommandAllocator> alloc;checked(dev->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&alloc)),"allocator");
    ComPtr<ID3D12GraphicsCommandList> list;checked(dev->CreateCommandList(0,qd.Type,alloc.Get(),pso,IID_PPV_ARGS(&list)),"list");
    list->SetComputeRootSignature(root);list->SetComputeRootUnorderedAccessView(0,gpu->GetGPUVirtualAddress());list->Dispatch(1,1,1);
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=gpu.Get();b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;list->ResourceBarrier(1,&b);list->CopyBufferRegion(read.Get(),0,gpu.Get(),0,bytes);checked(list->Close(),"close");
    ComPtr<ID3D12Fence> fence;checked(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");Event event;check(event.h!=nullptr,"event");
    ID3D12CommandList* commands[]{list.Get()};queue->ExecuteCommandLists(1,commands);
    if(FAILED(queue->Signal(fence.Get(),1))||FAILED(fence->SetEventOnCompletion(1,event.h))||WaitForSingleObject(event.h,10000)!=WAIT_OBJECT_0)abort_pending("uncertain GPU completion");
    const auto completed=fence->GetCompletedValue();if(completed==UINT64_MAX||completed<1)abort_pending("invalid fence completion");checked(dev->GetDeviceRemovedReason(),"device removed");
    const D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)},none{0,0};void* ptr=nullptr;checked(read->Map(0,&range,&ptr),"map");const auto* data=static_cast<const std::uint32_t*>(ptr);bool same=true;for(UINT i=0;i<count;++i)if(data[i]!=(((i+1u)*17u)^0xa5a5a5a5u))same=false;read->Unmap(0,&none);check(same,"compute output mismatch");
}
int run(bool hardware){
    ComPtr<IDXGIFactory4> factory;checked(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"factory");ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> dev;
    if(hardware){for(UINT i=0;;++i){ComPtr<IDXGIAdapter1> a;auto hr=factory->EnumAdapters1(i,&a);if(hr==DXGI_ERROR_NOT_FOUND)break;checked(hr,"enumerate");DXGI_ADAPTER_DESC1 d{};checked(a->GetDesc1(&d),"description");if((d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)||d.VendorId!=0x10de)continue;if(SUCCEEDED(D3D12CreateDevice(a.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)))){adapter=a;break;}}}
    else if(SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))))D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev));
    if(!dev){std::cout<<"SKIP: requested adapter unavailable\n";return 77;}
    DXGI_ADAPTER_DESC1 ad{};checked(adapter->GetDesc1(&ad),"adapter");std::wcerr<<L"adapter: "<<ad.Description<<L'\n';
    constexpr char shader[]="RWStructuredBuffer<uint> outbuf:register(u0); [numthreads(64,1,1)] void main(uint3 id:SV_DispatchThreadID){outbuf[id.x]=((id.x+1u)*17u)^0xA5A5A5A5u;}";
    ComPtr<ID3DBlob> code,errors;checked(D3DCompile(shader,sizeof(shader)-1,"original-cache-fixture",nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&errors),"compile");
    D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;param.Descriptor.ShaderRegister=0;param.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=1;rd.pParameters=&param;ComPtr<ID3DBlob> sig;checked(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&errors),"serialize");
    ComPtr<ID3D12RootSignature> root;checked(dev->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&root)),"root");
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};desc.pRootSignature=root.Get();desc.CS={code->GetBufferPointer(),code->GetBufferSize()};
    const auto key=compute_key(blob_bytes(sig.Get()),blob_bytes(code.Get()),desc.NodeMask,static_cast<std::uint32_t>(desc.Flags));check(!key.empty(),"key");
    // This temporary directory is exclusive and removed before exit. No
    // persisted data from prior runs or other adapters is loaded here.
    const auto path=std::filesystem::temp_directory_path()/("nativex-pso-fixture-"+std::to_string(Clock::now().time_since_epoch().count()));check(std::filesystem::create_directory(path),"temp dir");Temp temp{path};
    auto file=path/"fixture.bin";const Bytes scope{'s','a','m','e','-','d','e','v','i','c','e',1};Cache cold(scope);ComPtr<ID3D12PipelineState> pso;long long elapsed=0;HRESULT last=S_OK;
    auto create=[&](const Bytes* cached){D3D12_COMPUTE_PIPELINE_STATE_DESC input=desc;if(cached){input.CachedPSO.pCachedBlob=cached->data();input.CachedPSO.CachedBlobSizeInBytes=cached->size();}
        pso.Reset();const auto start=Clock::now();last=dev->CreateComputePipelineState(&input,IID_PPV_ARGS(&pso));elapsed=std::chrono::duration_cast<std::chrono::microseconds>(Clock::now()-start).count();
        if(FAILED(last))return Attempt{cached&&cache_rejection(last)?AttemptStatus::CacheRejected:AttemptStatus::Failed,{}};
        if(!cached){ComPtr<ID3DBlob> blob;const auto result=pso->GetCachedBlob(&blob);if(SUCCEEDED(result)&&blob&&blob->GetBufferSize()&&blob->GetBufferSize()<=MaxBlobBytes)return Attempt{AttemptStatus::Created,blob_bytes(blob.Get())};}
        return Attempt{AttemptStatus::Created,{}};};
    auto first=create_cached(cold,key,create);checked(last,"cold PSO");check(first.path==BuildPath::Cold&&pso,"cold creation");const auto cold_us=elapsed;dispatch_and_verify(dev.Get(),root.Get(),pso.Get());
    if(first.write!=WriteStatus::Stored){std::cout<<"SKIP: cold output verified, reusable blob unavailable\n";return 77;}
    check(cold.save(file)==Status::Ok,"save");pso.Reset();Cache warm(scope);check(warm.load(file)==Status::Ok,"reopen");auto second=create_cached(warm,key,create);checked(last,"warm PSO");check(pso.Get()!=nullptr,"warm object");dispatch_and_verify(dev.Get(),root.Get(),pso.Get());
    check(second.path==BuildPath::Hit&&second.attempts==1,"driver did not accept same-device cached PSO");
    std::cout<<"{\"cold_create_us\":"<<cold_us<<",\"cached_create_us\":"<<elapsed<<",\"verified_elements\":128,\"cache_accepted\":true,\"hardware\":"<<(hardware?"true":"false")<<"}\n";return 0;
}
}
int main(int argc,char** argv){try{if(argc!=2||(std::string(argv[1])!="warp"&&std::string(argv[1])!="nvidia"))return 2;return run(std::string(argv[1])=="nvidia");}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
