// Original Windows x64 BC1/BC3/BC4/BC5/BC7-mode6 hardware-format GPU decoding versus native CPU oracle.
// D3D12 WARP DirectX texture upload + shader Load + 256-pixel readback,
// not Xbox GDK/D3D12.X shader or physical GPU verification.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "native_bc_texture.hpp"
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
using scarlett::texture::Format;
namespace {
constexpr UINT SourceWidth=4, SourceHeight=4, TargetWidth=16,TargetHeight=16;
constexpr UINT SampleScale=4;
int fail(const char* action,HRESULT hr){
    std::cerr<<"D3D12 BC gpu/cpu differential "<<action<<" failed hr=0x"
             <<std::hex<<static_cast<unsigned long>(hr)<<'\n';
    return 1;
}
int skip(const char* what,HRESULT hr){
    std::cout<<"SKIP: "<<what<<" hr=0x"<<std::hex
             <<static_cast<unsigned long>(hr)<<'\n';
    return 77;
}
D3D12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE kind){
    D3D12_HEAP_PROPERTIES p{};
    p.Type=kind;p.CreationNodeMask=1;p.VisibleNodeMask=1;return p;
}
D3D12_RESOURCE_DESC buffer(UINT64 bytes){
    D3D12_RESOURCE_DESC d{};
    d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width=bytes;
    d.Height=1;
    d.DepthOrArraySize=1;
    d.MipLevels=1;
    d.SampleDesc.Count=1;
    d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}
D3D12_RESOURCE_DESC texture(UINT width,UINT height,DXGI_FORMAT format,
                            D3D12_RESOURCE_FLAGS flags){
    D3D12_RESOURCE_DESC d{};
    d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width=width;d.Height=height;
    d.DepthOrArraySize=1;
    d.MipLevels=1;
    d.Format=format;
    d.SampleDesc.Count=1;
    d.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d.Flags=flags;
    return d;
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource,
                                  D3D12_RESOURCE_STATES before,
                                  D3D12_RESOURCE_STATES after){
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=resource;
    barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore=before;
    barrier.Transition.StateAfter=after;
    return barrier;
}
void put16(std::uint8_t* p,unsigned value){
    p[0]=static_cast<std::uint8_t>(value);
    p[1]=static_cast<std::uint8_t>(value>>8u);
}
void put32(std::uint8_t* p,std::uint32_t value){
    for(unsigned i=0;i<4;i++)p[i]=static_cast<std::uint8_t>(value>>(i*8u));
}
struct Bytes {
    std::array<std::uint8_t,16> compressed{};
    std::array<std::uint8_t,64> expected{};
    std::size_t block_bytes=0;
};
Bytes make_block(Format kind){
    Bytes b{};
    const bool bc3=kind==Format::Bc3;
    const bool bc4=kind==Format::Bc4;
    const bool bc5=kind==Format::Bc5;
    const bool bc7=kind==Format::Bc7Mode6;
    b.block_bytes=(bc3 || bc5 || bc7)?16u:8u;
    if(bc7) {
        // Independently encoded mode6 BPTC 128-bit block; no console assets.
        constexpr std::array<std::uint8_t,16> Mode6{{
            0xc0,0x3f,0x00,0xf0,0x07,0xfc,0xff,0x80,
            0xf0,0x78,0x10,0x32,0x54,0x76,0x98,0xba
        }};
        b.compressed=Mode6;
    }else if(bc4 || bc5){
        // Independent BC4 UNORM blocks with nontrivial 3-bit selectors.
        // R uses 8-interpolated mode, G uses 6-interpolated mode.
        b.compressed[0]=255u;b.compressed[1]=0u;
        if(bc5){b.compressed[8]=0u;b.compressed[9]=255u;}
        std::uint64_t selector_bits=0;
        constexpr unsigned selections[4]={0u,1u,2u,7u};
        for(unsigned i=0;i<16;i++)
            selector_bits|=static_cast<std::uint64_t>(selections[i%4u])<<(3u*i);
        for(unsigned i=0;i<6;i++){
            b.compressed[2u+i]=static_cast<std::uint8_t>(selector_bits>>(8u*i));
            if(bc5)
                b.compressed[10u+i]=static_cast<std::uint8_t>(selector_bits>>(8u*i));
        }
    }else{
        std::uint8_t* colors=b.compressed.data()+(bc3?8u:0u);
        if(bc3){
            // BC3 has 8/6 alpha indices and always four RGB palette entries.
            b.compressed[0]=255u;b.compressed[1]=0u;
            std::uint64_t alpha_bits=0;
            constexpr unsigned choices[4]={0,1,2,7};
            for(unsigned i=0;i<16;i++)
                alpha_bits|=static_cast<std::uint64_t>(choices[i%4u])<<(3u*i);
            for(unsigned i=0;i<6;i++)
                b.compressed[i+2u]=static_cast<std::uint8_t>(alpha_bits>>(8u*i));
            put16(colors,0u);put16(colors+2u,0xffffu);
        }else{
            put16(colors,0xf800u);put16(colors+2u,0x001fu);
        }
        put32(colors+4u,0xe4e4e4e4u);
    }
    const scarlett::texture::Surface src{b.compressed.data(),b.block_bytes,
                                         b.block_bytes,SourceWidth,SourceHeight};
    const scarlett::texture::Output dst{b.expected.data(),b.expected.size(),
                                        SourceWidth*4u};
    if(scarlett::texture::decode(src,dst,kind)!=scarlett::texture::Status::Ok)
        throw std::runtime_error("independent CPU BC decoder rejected original block");
    return b;
}
bool close_channel(std::uint8_t a,std::uint8_t b){
    const unsigned x=a,y=b;
    return x+2u>=y&&x<=y+2u;
}
int run(Format kind){
    Bytes original=make_block(kind);
    DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    switch(kind){
        case Format::Bc1:format=DXGI_FORMAT_BC1_UNORM;break;
        case Format::Bc3:format=DXGI_FORMAT_BC3_UNORM;break;
        case Format::Bc4:format=DXGI_FORMAT_BC4_UNORM;break;
        case Format::Bc5:format=DXGI_FORMAT_BC5_UNORM;break;
        case Format::Bc7Mode6:format=DXGI_FORMAT_BC7_UNORM;break;
    }
    if(format==DXGI_FORMAT_UNKNOWN)return 2;
    HRESULT hr=S_OK;
    ComPtr<IDXGIFactory4> factory;
    hr=CreateDXGIFactory2(0,IID_PPV_ARGS(&factory));
    if(FAILED(hr))return skip("no DXGI factory",hr);
    ComPtr<IDXGIAdapter> warp;
    hr=factory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
    if(FAILED(hr))return skip("no WARP adapter",hr);
    ComPtr<ID3D12Device> device;
    hr=D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,
                          IID_PPV_ARGS(&device));
    if(FAILED(hr))return skip("no D3D12 WARP device",hr);
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{};
    support.Format=format;
    hr=device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,
                                   &support,sizeof(support));
    constexpr unsigned required=D3D12_FORMAT_SUPPORT1_TEXTURE2D|
                                 D3D12_FORMAT_SUPPORT1_SHADER_LOAD;
    if(FAILED(hr)||(static_cast<unsigned>(support.Support1)&required)!=required)
        return skip("WARP does not expose BC compressed texture shader Load",hr);

    const char* hlsl=
        "struct VOut {float4 p:SV_POSITION;};\n"
        "VOut vs(uint id:SV_VertexID){\n"
        "float2 points[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};\n"
        "VOut o;o.p=float4(points[id],0,1);return o;}\n"
        "Texture2D<float4> t0:register(t0);\n"
        "float4 ps(float4 p:SV_POSITION):SV_TARGET{\n"
        "uint2 coords=uint2(p.xy);return t0.Load(int3(coords/4,0));}\n";
    ComPtr<ID3DBlob> vs,ps,errors;
    hr=D3DCompile(hlsl,std::strlen(hlsl),"original-bc-shader",nullptr,
                  nullptr,"vs","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,
                  0,&vs,&errors);
    if(FAILED(hr))return fail("compile textured VS",hr);
    errors.Reset();
    hr=D3DCompile(hlsl,std::strlen(hlsl),"original-bc-shader",nullptr,
                  nullptr,"ps","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,
                  0,&ps,&errors);
    if(FAILED(hr))return fail("compile textured PS",hr);

    D3D12_DESCRIPTOR_RANGE descriptor{};
    descriptor.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptor.NumDescriptors=1;
    descriptor.BaseShaderRegister=0;
    descriptor.OffsetInDescriptorsFromTableStart=
        D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    D3D12_ROOT_PARAMETER parameter{};
    parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameter.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    parameter.DescriptorTable.NumDescriptorRanges=1;
    parameter.DescriptorTable.pDescriptorRanges=&descriptor;
    D3D12_ROOT_SIGNATURE_DESC root_desc{};
    root_desc.NumParameters=1;
    root_desc.pParameters=&parameter;
    root_desc.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ComPtr<ID3DBlob> root_bytes,root_errors;
    hr=D3D12SerializeRootSignature(&root_desc,D3D_ROOT_SIGNATURE_VERSION_1,
                                    &root_bytes,&root_errors);
    if(FAILED(hr))return fail("serialize textured root signature",hr);
    ComPtr<ID3D12RootSignature> root;
    hr=device->CreateRootSignature(0,root_bytes->GetBufferPointer(),
             root_bytes->GetBufferSize(),IID_PPV_ARGS(&root));
    if(FAILED(hr))return fail("create textured root signature",hr);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature=root.Get();
    pso_desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};
    pso_desc.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    pso_desc.SampleMask=UINT_MAX;
    pso_desc.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    pso_desc.RasterizerState.DepthBias=D3D12_DEFAULT_DEPTH_BIAS;
    pso_desc.RasterizerState.DepthBiasClamp=D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    pso_desc.RasterizerState.SlopeScaledDepthBias=
         D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
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
    hr=device->CreateGraphicsPipelineState(&pso_desc,IID_PPV_ARGS(&pso));
    if(FAILED(hr))return fail("create textured PSO",hr);

    const auto default_heap=heap(D3D12_HEAP_TYPE_DEFAULT);
    const auto upload_heap=heap(D3D12_HEAP_TYPE_UPLOAD);
    const auto read_heap=heap(D3D12_HEAP_TYPE_READBACK);
    const auto compressed_desc=texture(SourceWidth,SourceHeight,format,
                                       D3D12_RESOURCE_FLAG_NONE);
    ComPtr<ID3D12Resource> compressed_texture;
    hr=device->CreateCommittedResource(&default_heap,D3D12_HEAP_FLAG_NONE,
             &compressed_desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
             IID_PPV_ARGS(&compressed_texture));
    if(FAILED(hr))return fail("create native BC texture",hr);
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT source_footprint{};
    UINT source_rows=0;
    UINT64 source_row_bytes=0,source_total=0;
    device->GetCopyableFootprints(&compressed_desc,0,1,0,&source_footprint,
                                  &source_rows,&source_row_bytes,&source_total);
    if(source_rows!=1u || source_row_bytes!=original.block_bytes ||
       source_footprint.Footprint.RowPitch<original.block_bytes ||
       !source_total || source_total>65536u){
        std::cerr<<"unexpected BC compressed subresource copy footprint\n";
        return 1;
    }
    ComPtr<ID3D12Resource> upload;
    const auto upload_desc=buffer(source_total);
    hr=device->CreateCommittedResource(&upload_heap,D3D12_HEAP_FLAG_NONE,
             &upload_desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
             IID_PPV_ARGS(&upload));
    if(FAILED(hr))return fail("create compressed block upload",hr);
    {
        D3D12_RANGE empty{0,0};
        void* mapped=nullptr;
        if(FAILED(hr=upload->Map(0,&empty,&mapped)))
            return fail("Map BC upload buffer",hr);
        std::memcpy(static_cast<std::uint8_t*>(mapped)+source_footprint.Offset,
                    original.compressed.data(),original.block_bytes);
        D3D12_RANGE written{static_cast<SIZE_T>(source_footprint.Offset),
                            static_cast<SIZE_T>(source_footprint.Offset+
                                                original.block_bytes)};
        upload->Unmap(0,&written);
    }

    D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc{};
    srv_heap_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srv_heap_desc.NumDescriptors=1;
    srv_heap_desc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> srv_heap;
    hr=device->CreateDescriptorHeap(&srv_heap_desc,IID_PPV_ARGS(&srv_heap));
    if(FAILED(hr))return fail("create shader-visible SRV heap",hr);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
    srv.Format=format;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels=1;
    device->CreateShaderResourceView(compressed_texture.Get(),&srv,
                                     srv_heap->GetCPUDescriptorHandleForHeapStart());

    const auto color_desc=texture(TargetWidth,TargetHeight,
                DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
    ComPtr<ID3D12Resource> render_target;
    hr=device->CreateCommittedResource(&default_heap,D3D12_HEAP_FLAG_NONE,
             &color_desc,D3D12_RESOURCE_STATE_RENDER_TARGET,nullptr,
             IID_PPV_ARGS(&render_target));
    if(FAILED(hr))return fail("create GPU output texture",hr);
    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
    rtv_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.NumDescriptors=1;
    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    hr=device->CreateDescriptorHeap(&rtv_desc,IID_PPV_ARGS(&rtv_heap));
    if(FAILED(hr))return fail("create RTV heap",hr);
    const auto rtv=rtv_heap->GetCPUDescriptorHandleForHeapStart();
    device->CreateRenderTargetView(render_target.Get(),nullptr,rtv);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT read_footprint{};
    UINT read_rows=0;
    UINT64 read_row_bytes=0,read_total=0;
    device->GetCopyableFootprints(&color_desc,0,1,0,&read_footprint,
                                  &read_rows,&read_row_bytes,&read_total);
    if(read_rows!=TargetHeight || read_row_bytes!=TargetWidth*4u ||
       read_total==0 || read_total>65536u){
        std::cerr<<"unexpected D3D12 output texture readback footprint\n";
        return 1;
    }
    ComPtr<ID3D12Resource> readback;
    const auto readback_desc=buffer(read_total);
    hr=device->CreateCommittedResource(&read_heap,D3D12_HEAP_FLAG_NONE,
             &readback_desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
             IID_PPV_ARGS(&readback));
    if(FAILED(hr))return fail("create RGBA8 readback",hr);
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    hr=device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue));
    if(FAILED(hr))return fail("create D3D12 graphics queue",hr);
    ComPtr<ID3D12CommandAllocator> allocator;
    hr=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                      IID_PPV_ARGS(&allocator));
    if(FAILED(hr))return fail("create command allocator",hr);
    ComPtr<ID3D12GraphicsCommandList> command;
    hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 allocator.Get(),pso.Get(),IID_PPV_ARGS(&command));
    if(FAILED(hr))return fail("create graphics command list",hr);

    D3D12_TEXTURE_COPY_LOCATION gpu_tex{},cpu_block{};
    gpu_tex.pResource=compressed_texture.Get();
    gpu_tex.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    gpu_tex.SubresourceIndex=0;
    cpu_block.pResource=upload.Get();
    cpu_block.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    cpu_block.PlacedFootprint=source_footprint;
    command->CopyTextureRegion(&gpu_tex,0,0,0,&cpu_block,nullptr);
    auto sample_ready=transition(compressed_texture.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    command->ResourceBarrier(1,&sample_ready);
    const D3D12_VIEWPORT viewport{0.f,0.f,
        static_cast<float>(TargetWidth),static_cast<float>(TargetHeight),0.f,1.f};
    const D3D12_RECT scissor{0,0,static_cast<LONG>(TargetWidth),
                                 static_cast<LONG>(TargetHeight)};
    command->RSSetViewports(1,&viewport);
    command->RSSetScissorRects(1,&scissor);
    const float initial_clear[4]={1.f,0.f,1.f,1.f};
    command->ClearRenderTargetView(rtv,initial_clear,0,nullptr);
    command->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
    command->SetPipelineState(pso.Get());
    command->SetGraphicsRootSignature(root.Get());
    ID3D12DescriptorHeap* heaps[]={srv_heap.Get()};
    command->SetDescriptorHeaps(1,heaps);
    command->SetGraphicsRootDescriptorTable(0,
                 srv_heap->GetGPUDescriptorHandleForHeapStart());
    command->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    command->DrawInstanced(3,1,0,0);

    auto copy_ready=transition(render_target.Get(),
                         D3D12_RESOURCE_STATE_RENDER_TARGET,
                         D3D12_RESOURCE_STATE_COPY_SOURCE);
    command->ResourceBarrier(1,&copy_ready);
    D3D12_TEXTURE_COPY_LOCATION gpu_color{},cpu_read{};
    gpu_color.pResource=render_target.Get();
    gpu_color.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    gpu_color.SubresourceIndex=0;
    cpu_read.pResource=readback.Get();
    cpu_read.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    cpu_read.PlacedFootprint=read_footprint;
    command->CopyTextureRegion(&cpu_read,0,0,0,&gpu_color,nullptr);
    if(FAILED(hr=command->Close()))return fail("close GPU BC sampling list",hr);
    ID3D12CommandList* batch[]={command.Get()};
    queue->ExecuteCommandLists(1,batch);
    ComPtr<ID3D12Fence> fence;
    hr=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence));
    if(FAILED(hr))return fail("create graphics fence",hr);
    if(FAILED(hr=queue->Signal(fence.Get(),1u)))
        return fail("signal graphics fence",hr);
    HANDLE finished=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!finished)return fail("CreateEventW",HRESULT_FROM_WIN32(GetLastError()));
    const HRESULT wait_hr=fence->SetEventOnCompletion(1u,finished);
    const DWORD result=SUCCEEDED(wait_hr)
                       ?WaitForSingleObject(finished,10000):WAIT_FAILED;
    CloseHandle(finished);
    if(FAILED(wait_hr))return fail("SetEventOnCompletion",wait_hr);
    if(result!=WAIT_OBJECT_0){std::cerr<<"GPU BC sampling timeout\n";return 1;}
    if(FAILED(hr=device->GetDeviceRemovedReason()))
        return fail("GPU removed after BC sampling",hr);

    D3D12_RANGE readable{0,static_cast<SIZE_T>(read_total)};
    void* ptr=nullptr;
    if(FAILED(hr=readback->Map(0,&readable,&ptr)))
        return fail("Map GPU decompressed pixels",hr);
    const auto* data=static_cast<const std::uint8_t*>(ptr);
    bool good=true;
    UINT pixels=0;
    for(UINT y=0;y<TargetHeight;y++){
        for(UINT x=0;x<TargetWidth;x++){
            const auto* actual=data+static_cast<std::size_t>(y)*
                                read_footprint.Footprint.RowPitch+x*4u;
            const auto* expected=original.expected.data()+
                     ((y/SampleScale)*SourceWidth+x/SampleScale)*4u;
            for(UINT channel=0;channel<4;channel++)
                if(!close_channel(actual[channel],expected[channel]))good=false;
            ++pixels;
        }
    }
    D3D12_RANGE no_writes{0,0};
    readback->Unmap(0,&no_writes);
    if(!good || pixels!=TargetWidth*TargetHeight){
        std::cerr<<"GPU BC format shader Load output differs from CPU reference\n";
        return 1;
    }
    std::cout<<"{\"kind\":\"native-warp-gpu-bc-format-versus-cpu\","
             "\"format\":\""<<(kind==Format::Bc1?"BC1":kind==Format::Bc3?"BC3":kind==Format::Bc4?"BC4":kind==Format::Bc5?"BC5":"BC7-mode6")<<"\","
             "\"texture_upload_and_srv\":true,"
             "\"shader_load_and_draw\":true,"
             "\"pixels_verified\":"<<pixels<<","
             "\"color_channels_checked\":"<<pixels*4u<<","
             "\"gpu_fence_completed\":true,"
             "\"cpu_reference_independent_pixel_tests\":true,"
             "\"real_hardware_gpu_tested\":false,"
             "\"console_swizzle_tested\":false,"
             "\"game_assets_tested\":false}\n";
    return 0;
}
}
int main(int argc,char** argv){
    if(argc!=2){std::cerr<<"Usage: d3d12_warp_bc_texture bc1|bc3|bc4|bc5|bc7-mode6\n";return 2;}
    try{
        if(std::strcmp(argv[1],"bc1")==0)return run(Format::Bc1);
        if(std::strcmp(argv[1],"bc3")==0)return run(Format::Bc3);
        if(std::strcmp(argv[1],"bc4")==0)return run(Format::Bc4);
        if(std::strcmp(argv[1],"bc5")==0)return run(Format::Bc5);
        if(std::strcmp(argv[1],"bc7-mode6")==0)return run(Format::Bc7Mode6);
        return 2;
    }catch(const std::exception& e){
        std::cerr<<"D3D12 BC texture fixture exception: "<<e.what()<<'\n';
        return 1;
    }
}
