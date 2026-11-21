// Source-built Windows D3D12 WARP raster draw + actual GPU texture readback.
// No game EXE, Xbox shader, proprietary API, hardware GPU or unknown binary.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>
using Microsoft::WRL::ComPtr;
namespace {
int fail(const char* operation, HRESULT hr) {
    std::cerr << operation << " HRESULT=0x" << std::hex
              << static_cast<unsigned long>(hr) << '\n';
    return 1;
}
D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES props{};
    props.Type=type;
    props.CreationNodeMask=1;
    props.VisibleNodeMask=1;
    return props;
}
D3D12_RESOURCE_DESC buffer(std::uint64_t size) {
    D3D12_RESOURCE_DESC d{};
    d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width=size;
    d.Height=1;
    d.DepthOrArraySize=1;
    d.MipLevels=1;
    d.SampleDesc.Count=1;
    d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}
bool approx(std::uint8_t actual, unsigned expected) {
    return static_cast<unsigned>(actual)+2u>=expected &&
           static_cast<unsigned>(actual)<=expected+2u;
}
bool pixel(const std::uint8_t* base,UINT pitch,UINT x,UINT y,
           unsigned red,unsigned green,unsigned blue,unsigned alpha) {
    const auto* p=base+static_cast<std::size_t>(y)*pitch+x*4u;
    return approx(p[0],red)&&approx(p[1],green)&&
           approx(p[2],blue)&&approx(p[3],alpha);
}
}
int main() {
    constexpr UINT width=16, height=16;
    HRESULT hr=S_OK;
    ComPtr<IDXGIFactory4> factory;
    if(FAILED(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)))) {
        std::cout << "SKIP: DXGI factory unavailable\n";
        return 77;
    }
    ComPtr<IDXGIAdapter> warp;
    if(FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)))) {
        std::cout << "SKIP: WARP adapter unavailable\n";
        return 77;
    }
    ComPtr<ID3D12Device> device;
    if(FAILED(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,
                                IID_PPV_ARGS(&device)))) {
        std::cout << "SKIP: D3D12 WARP unavailable\n";
        return 77;
    }

    const char* hlsl=
        "float4 vs(uint id:SV_VertexID):SV_POSITION {\n"
        " float2 p[3]={float2(-0.9,-0.9),float2(0.0,0.9),float2(0.9,-0.9)};\n"
        " return float4(p[id],0,1);\n"
        "}\n"
        "float4 ps():SV_TARGET { return float4(0.25,0.50,1.0,1.0); }\n";
    ComPtr<ID3DBlob> vertex,pixel_shader,errors;
    hr=D3DCompile(hlsl,std::strlen(hlsl),"synthetic-raster",nullptr,
                  nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,
                  0,&vertex,&errors);
    if(FAILED(hr))return fail("compile VS",hr);
    errors.Reset();
    hr=D3DCompile(hlsl,std::strlen(hlsl),"synthetic-raster",nullptr,
                  nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,
                  0,&pixel_shader,&errors);
    if(FAILED(hr))return fail("compile PS",hr);

    D3D12_ROOT_SIGNATURE_DESC root_desc{};
    root_desc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> serialized,root_error;
    if(FAILED(hr=D3D12SerializeRootSignature(&root_desc,
              D3D_ROOT_SIGNATURE_VERSION_1,&serialized,&root_error)))
        return fail("serialize root signature",hr);
    ComPtr<ID3D12RootSignature> root;
    if(FAILED(hr=device->CreateRootSignature(0,serialized->GetBufferPointer(),
                        serialized->GetBufferSize(),IID_PPV_ARGS(&root))))
        return fail("create root signature",hr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature=root.Get();
    pso_desc.VS={vertex->GetBufferPointer(),vertex->GetBufferSize()};
    pso_desc.PS={pixel_shader->GetBufferPointer(),pixel_shader->GetBufferSize()};
    pso_desc.SampleMask=UINT_MAX;
    pso_desc.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    pso_desc.RasterizerState.DepthBias=D3D12_DEFAULT_DEPTH_BIAS;
    pso_desc.RasterizerState.DepthBiasClamp=D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    pso_desc.RasterizerState.SlopeScaledDepthBias=D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    pso_desc.RasterizerState.DepthClipEnable=TRUE;
    pso_desc.BlendState.RenderTarget[0].RenderTargetWriteMask=
        D3D12_COLOR_WRITE_ENABLE_ALL;
    pso_desc.DepthStencilState.DepthEnable=FALSE;
    pso_desc.DepthStencilState.StencilEnable=FALSE;
    pso_desc.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets=1;
    pso_desc.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;
    pso_desc.SampleDesc.Count=1;
    ComPtr<ID3D12PipelineState> pso;
    if(FAILED(hr=device->CreateGraphicsPipelineState(&pso_desc,IID_PPV_ARGS(&pso))))
        return fail("create graphics PSO",hr);

    D3D12_RESOURCE_DESC texture{};
    texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width=width;
    texture.Height=height;
    texture.DepthOrArraySize=1;
    texture.MipLevels=1;
    texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.SampleDesc.Count=1;
    texture.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    const auto default_heap=heap(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> render_target;
    if(FAILED(hr=device->CreateCommittedResource(&default_heap,D3D12_HEAP_FLAG_NONE,
          &texture,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,
          IID_PPV_ARGS(&render_target))))
        return fail("create render texture",hr);

    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc{};
    rtv_heap_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.NumDescriptors=1;
    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    if(FAILED(hr=device->CreateDescriptorHeap(&rtv_heap_desc,IID_PPV_ARGS(&rtv_heap))))
        return fail("create RTV descriptor heap",hr);
    const auto rtv=rtv_heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(render_target.Get(),nullptr,rtv);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows=0;
    UINT64 row_bytes=0,total_bytes=0;
    device->GetCopyableFootprints(&texture,0,1,0,&footprint,&rows,
                                 &row_bytes,&total_bytes);
    if(rows!=height || row_bytes!=width*4u || total_bytes==0) {
        std::cerr << "unexpected copyable texture layout\n"; return 1;
    }
    const auto read_heap=heap(D3D12_HEAP_TYPE_READBACK);
    const auto read_desc=buffer(total_bytes);
    ComPtr<ID3D12Resource> readback;
    if(FAILED(hr=device->CreateCommittedResource(&read_heap,D3D12_HEAP_FLAG_NONE,
          &read_desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
          IID_PPV_ARGS(&readback))))
        return fail("create readback",hr);

    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if(FAILED(hr=device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue))))
        return fail("create direct queue",hr);
    ComPtr<ID3D12CommandAllocator> allocator;
    if(FAILED(hr=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&allocator))))
        return fail("create allocator",hr);
    ComPtr<ID3D12GraphicsCommandList> commands;
    if(FAILED(hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
                        allocator.Get(),pso.Get(),IID_PPV_ARGS(&commands))))
        return fail("create command list",hr);

    D3D12_VIEWPORT viewport{0.f,0.f,static_cast<float>(width),
                            static_cast<float>(height),0.f,1.f};
    D3D12_RECT scissor{0,0,static_cast<LONG>(width),static_cast<LONG>(height)};
    const float clear[4]={0.f,0.f,0.f,1.f};
    commands->RSSetViewports(1,&viewport);
    commands->RSSetScissorRects(1,&scissor);
    commands->ClearRenderTargetView(rtv,clear,0,nullptr);
    commands->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
    commands->SetGraphicsRootSignature(root.Get());
    commands->SetPipelineState(pso.Get());
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->DrawInstanced(3,1,0,0);

    D3D12_RESOURCE_BARRIER transition{};
    transition.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    transition.Transition.pResource=render_target.Get();
    transition.Transition.StateBefore=D3D12_RESOURCE_STATE_RENDER_TARGET;
    transition.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    transition.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commands->ResourceBarrier(1,&transition);
    D3D12_TEXTURE_COPY_LOCATION source{},destination{};
    source.pResource=render_target.Get();
    source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex=0;
    destination.pResource=readback.Get();
    destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint=footprint;
    commands->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
    if(FAILED(hr=commands->Close()))return fail("close command list",hr);

    ID3D12CommandList* batch[]={commands.Get()};
    queue->ExecuteCommandLists(1,batch);
    ComPtr<ID3D12Fence> fence;
    if(FAILED(hr=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))
        return fail("create fence",hr);
    if(FAILED(hr=queue->Signal(fence.Get(),1)))return fail("signal fence",hr);
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return fail("CreateEventW",HRESULT_FROM_WIN32(GetLastError()));
    const HRESULT wait_hr=fence->SetEventOnCompletion(1,event);
    const DWORD waited=SUCCEEDED(wait_hr)?WaitForSingleObject(event,10000):WAIT_FAILED;
    CloseHandle(event);
    if(FAILED(wait_hr))return fail("SetEventOnCompletion",wait_hr);
    if(waited!=WAIT_OBJECT_0){std::cerr<<"raster readback timed out\n";return 1;}
    if(FAILED(hr=device->GetDeviceRemovedReason()))return fail("device removed",hr);
    D3D12_RANGE readable{0,static_cast<SIZE_T>(total_bytes)};
    void* mapped=nullptr;
    if(FAILED(hr=readback->Map(0,&readable,&mapped)))return fail("map readback",hr);
    const auto* data=static_cast<const std::uint8_t*>(mapped);
    const bool triangle=pixel(data,footprint.Footprint.RowPitch,8,8,64,128,255,255);
    const bool background=pixel(data,footprint.Footprint.RowPitch,0,0,0,0,0,255);
    D3D12_RANGE no_writes{0,0};
    readback->Unmap(0,&no_writes);
    if(!triangle || !background){
        std::cerr<<"offscreen raster output did not match expected triangle/background\n";
        return 1;
    }
    std::cout << "{\"kind\":\"native-d3d12-warp-raster-readback\","
              << "\"graphics_draw\":true,\"pixels_verified\":true,"
              << "\"present_tested\":false,\"hardware_gpu_tested\":false,"
              << "\"xbox_d3d12x_tested\":false,\"game_tested\":false}\n";
    return 0;
}
