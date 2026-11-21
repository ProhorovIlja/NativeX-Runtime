// Source-built Windows D3D12/WARP compute and CPU readback fixture.
// No proprietary shader, console code, game asset or hardware GPU requirement.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include <iostream>
using Microsoft::WRL::ComPtr;

static int failure(const char* step,HRESULT hr){
    std::cerr<<step<<" HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';
    return 1;
}
static D3D12_RESOURCE_DESC buffer_desc(uint64_t bytes,D3D12_RESOURCE_FLAGS flags){
    D3D12_RESOURCE_DESC d{};
    d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width=bytes;
    d.Height=1;
    d.DepthOrArraySize=1;
    d.MipLevels=1;
    d.SampleDesc.Count=1;
    d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d.Flags=flags;
    return d;
}
static D3D12_HEAP_PROPERTIES heap_props(D3D12_HEAP_TYPE type){
    D3D12_HEAP_PROPERTIES h{};
    h.Type=type;
    h.CPUPageProperty=D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    h.MemoryPoolPreference=D3D12_MEMORY_POOL_UNKNOWN;
    h.CreationNodeMask=1;
    h.VisibleNodeMask=1;
    return h;
}
int main(){
    constexpr uint32_t count=64;
    constexpr uint64_t bytes=count*sizeof(uint32_t);
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr=CreateDXGIFactory2(0,IID_PPV_ARGS(&factory));
    if(FAILED(hr)){std::cout<<"SKIP: DXGI factory unavailable\n";return 77;}
    ComPtr<IDXGIAdapter> adapter;
    if(FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)))){
        std::cout<<"SKIP: WARP adapter unavailable\n";return 77;
    }
    ComPtr<ID3D12Device> device;
    if(FAILED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device)))){
        std::cout<<"SKIP: D3D12 WARP not supported\n";return 77;
    }

    const char shader[] =
        "RWStructuredBuffer<uint> outbuf : register(u0);\n"
        "[numthreads(64,1,1)]\n"
        "void main(uint3 id:SV_DispatchThreadID) {\n"
        " uint x=id.x; outbuf[x]=((x+1u)*17u)^0xA5A5A5A5u;\n"
        "}\n";
    ComPtr<ID3DBlob> bytecode,errors;
    hr=D3DCompile(shader,std::strlen(shader),"synthetic-wave-free-compute",nullptr,nullptr,
                  "main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&bytecode,&errors);
    if(FAILED(hr)){
        if(errors)std::cerr.write(static_cast<const char*>(errors->GetBufferPointer()),
                                 static_cast<std::streamsize>(errors->GetBufferSize()));
        return failure("D3DCompile",hr);
    }

    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
    parameter.Descriptor.ShaderRegister=0;
    parameter.Descriptor.RegisterSpace=0;
    parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC root_desc{};
    root_desc.NumParameters=1;
    root_desc.pParameters=&parameter;
    ComPtr<ID3DBlob> serialized,root_error;
    hr=D3D12SerializeRootSignature(&root_desc,D3D_ROOT_SIGNATURE_VERSION_1,
                                   &serialized,&root_error);
    if(FAILED(hr))return failure("D3D12SerializeRootSignature",hr);
    ComPtr<ID3D12RootSignature> root;
    hr=device->CreateRootSignature(0,serialized->GetBufferPointer(),
          serialized->GetBufferSize(),IID_PPV_ARGS(&root));
    if(FAILED(hr))return failure("CreateRootSignature",hr);
    D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline{};
    pipeline.pRootSignature=root.Get();
    pipeline.CS.pShaderBytecode=bytecode->GetBufferPointer();
    pipeline.CS.BytecodeLength=bytecode->GetBufferSize();
    ComPtr<ID3D12PipelineState> pso;
    if(FAILED(hr=device->CreateComputePipelineState(&pipeline,IID_PPV_ARGS(&pso))))
        return failure("CreateComputePipelineState",hr);
    const auto gpu_heap=heap_props(D3D12_HEAP_TYPE_DEFAULT);
    const auto read_heap=heap_props(D3D12_HEAP_TYPE_READBACK);
    const auto gpu_desc=buffer_desc(bytes,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    const auto read_desc=buffer_desc(bytes,D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> output,readback;
    if(FAILED(hr=device->CreateCommittedResource(&gpu_heap,D3D12_HEAP_FLAG_NONE,&gpu_desc,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output))))
        return failure("Create GPU buffer",hr);
    if(FAILED(hr=device->CreateCommittedResource(&read_heap,D3D12_HEAP_FLAG_NONE,&read_desc,
                    D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback))))
        return failure("Create readback buffer",hr);

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type=D3D12_COMMAND_LIST_TYPE_COMPUTE;
    ComPtr<ID3D12CommandQueue> queue;
    if(FAILED(hr=device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue))))
        return failure("CreateCommandQueue",hr);
    ComPtr<ID3D12CommandAllocator> allocator;
    if(FAILED(hr=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                                IID_PPV_ARGS(&allocator))))
        return failure("CreateCommandAllocator",hr);
    ComPtr<ID3D12GraphicsCommandList> list;
    if(FAILED(hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_COMPUTE,
              allocator.Get(),pso.Get(),IID_PPV_ARGS(&list))))
        return failure("CreateCommandList",hr);
    list->SetComputeRootSignature(root.Get());
    list->SetPipelineState(pso.Get());
    list->SetComputeRootUnorderedAccessView(0,output->GetGPUVirtualAddress());
    list->Dispatch(1,1,1);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=output.Get();
    barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    list->ResourceBarrier(1,&barrier);
    list->CopyBufferRegion(readback.Get(),0,output.Get(),0,bytes);
    if(FAILED(hr=list->Close()))return failure("Close command list",hr);
    ID3D12CommandList* submitted[]={list.Get()};
    queue->ExecuteCommandLists(1,submitted);

    ComPtr<ID3D12Fence> fence;
    if(FAILED(hr=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))
        return failure("CreateFence",hr);
    if(FAILED(hr=queue->Signal(fence.Get(),1)))return failure("Signal",hr);
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return failure("CreateEvent",HRESULT_FROM_WIN32(GetLastError()));
    HRESULT wait_hr=fence->SetEventOnCompletion(1,event);
    DWORD waited=SUCCEEDED(wait_hr)?WaitForSingleObject(event,10000):WAIT_FAILED;
    CloseHandle(event);
    if(FAILED(wait_hr)||waited!=WAIT_OBJECT_0){
        std::cerr<<"D3D12 compute fence timeout or error\n";
        return 1;
    }
    if(FAILED(hr=device->GetDeviceRemovedReason()))return failure("DeviceRemoved",hr);
    D3D12_RANGE range{0,static_cast<SIZE_T>(bytes)};
    void* mapped=nullptr;
    if(FAILED(hr=readback->Map(0,&range,&mapped)))return failure("Map readback",hr);
    const auto* observed=static_cast<const uint32_t*>(mapped);
    unsigned mismatch=0;
    for(uint32_t i=0;i<count;++i){
        const uint32_t expected=((i+1u)*17u)^0xA5A5A5A5u;
        if(observed[i]!=expected){
            ++mismatch;
            if(mismatch<=3)std::cerr<<"Compute mismatch lane "<<i<<'\n';
        }
    }
    D3D12_RANGE written{0,0};
    readback->Unmap(0,&written);
    if(mismatch)return 1;
    std::cout<<"{\"kind\":\"native-d3d12-warp-compute\","
               "\"compute_dispatch\":true,\"readback_verified\":true,"
               "\"verified_elements\":64,\"wave_intrinsics_tested\":false,"
               "\"rdna2_tested\":false,\"xbox_title_tested\":false}\n";
    return 0;
}
