// Original native Win32 D3D12 GPU tile aliasing and readback of two virtual pages.
// Two virtual 64 KiB pages deliberately alias one physical 64 KiB heap page.
// This is NOT an Xbox UMA or GPU-CPU guest address translator.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT TileBytes=D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;
constexpr UINT TileCount=2;
constexpr UINT TotalBytes=TileBytes*TileCount;
int fail(const char* step,HRESULT hr) {
    std::cerr<<step<<" HRESULT=0x"<<std::hex
             <<static_cast<unsigned long>(hr)<<'\n';return 1;
}
int skip(const char* reason) {
    std::cout<<"SKIP: "<<reason<<'\n';return 77;
}
D3D12_HEAP_PROPERTIES memory_props(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES p{};
    p.Type=type;
    p.CreationNodeMask=1;
    p.VisibleNodeMask=1;
    return p;
}
D3D12_RESOURCE_DESC buffer(UINT64 bytes) {
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
std::uint8_t expected(UINT offset){
    return static_cast<std::uint8_t>(((offset%TileBytes)*17u+11u)%251u);
}
int run(){
    ComPtr<IDXGIFactory4> factory;
    if(FAILED(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory))))
        return skip("DXGI factory unavailable");
    ComPtr<IDXGIAdapter> warp;
    if(FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp))))
        return skip("WARP adapter unavailable");
    ComPtr<ID3D12Device> device;
    if(FAILED(D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,
                                IID_PPV_ARGS(&device))))
        return skip("D3D12 software adapter unavailable");
    D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
    if(FAILED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
                                          &options,sizeof(options))) ||
       options.TiledResourcesTier<D3D12_TILED_RESOURCES_TIER_1)
        return skip("D3D12 tiled resource tier not supported");
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    HRESULT hr=device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue));
    if(FAILED(hr))return fail("CreateCommandQueue",hr);
    D3D12_HEAP_DESC tiles{};
    tiles.SizeInBytes=TileBytes; // exactly ONE physical tile for TWO virtual tiles
    tiles.Properties=memory_props(D3D12_HEAP_TYPE_DEFAULT);
    tiles.Alignment=D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    tiles.Flags=D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
    ComPtr<ID3D12Heap> tile_heap;
    hr=device->CreateHeap(&tiles,IID_PPV_ARGS(&tile_heap));
    if(FAILED(hr))return fail("CreateHeap",hr);
    const auto desc=buffer(TotalBytes);
    ComPtr<ID3D12Resource> reserved;
    hr=device->CreateReservedResource(&desc,D3D12_RESOURCE_STATE_COPY_DEST,
                                      nullptr,IID_PPV_ARGS(&reserved));
    if(FAILED(hr))return skip("D3D12 WARP cannot create reserved 64 KiB buffer");
    UINT actual_tiles=0;
    device->GetResourceTiling(reserved.Get(),&actual_tiles,
                               nullptr,nullptr,nullptr,0,nullptr);
    if(actual_tiles!=TileCount){
        std::cerr<<"unexpected D3D12 tile count="<<actual_tiles<<'\n';return 1;
    }
    D3D12_TILED_RESOURCE_COORDINATE begin{};
    D3D12_TILE_REGION_SIZE size{};
    size.NumTiles=TileCount;
    size.UseBox=FALSE;
    const D3D12_TILE_RANGE_FLAGS flags=D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE;
    const UINT heap_start=0;
    const UINT tile_count=TileCount;
    // Schedules tile mapping on the actual native D3D12 command queue.
    queue->UpdateTileMappings(reserved.Get(),1,&begin,&size,tile_heap.Get(),
                              1,&flags,&heap_start,&tile_count,
                              D3D12_TILE_MAPPING_FLAG_NONE);
    ComPtr<ID3D12Resource> upload,readback;
    const auto upload_heap=memory_props(D3D12_HEAP_TYPE_UPLOAD);
    const auto read_heap=memory_props(D3D12_HEAP_TYPE_READBACK);
    hr=device->CreateCommittedResource(&upload_heap,D3D12_HEAP_FLAG_NONE,
         &desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
         IID_PPV_ARGS(&upload));
    if(FAILED(hr))return fail("create upload",hr);
    hr=device->CreateCommittedResource(&read_heap,D3D12_HEAP_FLAG_NONE,
         &desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
         IID_PPV_ARGS(&readback));
    if(FAILED(hr))return fail("create readback",hr);
    void* upload_cpu=nullptr;
    const D3D12_RANGE no_read{0,0};
    hr=upload->Map(0,&no_read,&upload_cpu);
    if(FAILED(hr))return fail("map upload",hr);
    auto* data=static_cast<std::uint8_t*>(upload_cpu);
    for(UINT i=0;i<TotalBytes;i++)data[i]=expected(i);
    const D3D12_RANGE written{0,TotalBytes};
    upload->Unmap(0,&written);
    ComPtr<ID3D12CommandAllocator> allocator;
    hr=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                       IID_PPV_ARGS(&allocator));
    if(FAILED(hr))return fail("CreateCommandAllocator",hr);
    ComPtr<ID3D12GraphicsCommandList> commands;
    hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
                                 allocator.Get(),nullptr,IID_PPV_ARGS(&commands));
    if(FAILED(hr))return fail("CreateCommandList",hr);
    // Write only the first virtual tile. The second maps to the SAME heap tile.
    commands->CopyBufferRegion(reserved.Get(),0,upload.Get(),0,TileBytes);
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource=reserved.Get();
    barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
    commands->ResourceBarrier(1,&barrier);
    commands->CopyBufferRegion(readback.Get(),0,reserved.Get(),0,TotalBytes);
    hr=commands->Close();
    if(FAILED(hr))return fail("Close",hr);
    ID3D12CommandList* jobs[]{commands.Get()};
    queue->ExecuteCommandLists(1,jobs);
    ComPtr<ID3D12Fence> fence;
    hr=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence));
    if(FAILED(hr))return fail("CreateFence",hr);
    hr=queue->Signal(fence.Get(),1);
    if(FAILED(hr))return fail("Signal",hr);
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return fail("CreateEventW",HRESULT_FROM_WIN32(GetLastError()));
    const HRESULT wait_hr=fence->SetEventOnCompletion(1,event);
    const DWORD waited=SUCCEEDED(wait_hr)?WaitForSingleObject(event,10000):WAIT_FAILED;
    CloseHandle(event);
    if(FAILED(wait_hr))return fail("SetEventOnCompletion",wait_hr);
    if(waited!=WAIT_OBJECT_0){std::cerr<<"tiled resource fence timeout\n";return 1;}
    hr=device->GetDeviceRemovedReason();
    if(FAILED(hr))return fail("GetDeviceRemovedReason",hr);
    void* mapped=nullptr;
    const D3D12_RANGE readable{0,TotalBytes};
    hr=readback->Map(0,&readable,&mapped);
    if(FAILED(hr))return fail("map readback",hr);
    const auto* output=static_cast<const std::uint8_t*>(mapped);
    std::uint64_t checksum=0;
    bool matched=true;
    for(UINT i=0;i<TotalBytes;i++){
        if(output[i]!=expected(i)){matched=false;break;}
        checksum+=output[i];
    }
    readback->Unmap(0,&no_read);
    if(!matched){std::cerr<<"tiled D3D12 WARP ALIAS readback mismatch\n";return 1;}
    std::cout<<"{\"kind\":\"native-win-d3d12-warp-tile-alias\","
             "\"tiled_resource_tier\":"<<static_cast<unsigned>(options.TiledResourcesTier)<<","
             "\"tile_bytes\":"<<TileBytes<<","
             "\"mapped_virtual_tiles\":"<<TileCount<<","
             "\"physical_heap_tiles\":1,"
             "\"readback_bytes_verified\":"<<TotalBytes<<","
             "\"checksum\":"<<checksum<<","
             "\"real_d3d12_tile_alias_mapping\":true,"
             "\"hardware_gpu_verified\":false,"
             "\"xbox_uma_address_translation_verified\":false,"
             "\"game_tested\":false}\n";
    return 0;
}
}
int main(){return run();}
