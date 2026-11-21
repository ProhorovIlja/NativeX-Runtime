// Original Windows-only D3D12 WARP execution smoke test.
// No Xbox API, no game assets, no third-party shader data, no GPU assumptions.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <iostream>
using Microsoft::WRL::ComPtr;

static int fail(const char* step,HRESULT hr){
    std::cerr << step << " HRESULT=0x" << std::hex << static_cast<unsigned long>(hr) << '\n';
    return 1;
}
int main(){
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr=CreateDXGIFactory2(0,IID_PPV_ARGS(&factory));
    if(FAILED(hr)){
        std::cout << "SKIP: host DXGI factory unavailable\n";
        return 77;
    }
    ComPtr<IDXGIAdapter> warp;
    hr=factory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
    if(FAILED(hr)){
        std::cout << "SKIP: DXGI WARP adapter unavailable\n";
        return 77;
    }
    ComPtr<ID3D12Device> device;
    hr=D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
    if(FAILED(hr)){
        std::cout << "SKIP: WARP adapter cannot create D3D12 device\n";
        return 77;
    }
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 options{};
    const bool wave_info=SUCCEEDED(device->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS1,&options,sizeof(options)));
    D3D12_COMMAND_QUEUE_DESC qdesc{};
    qdesc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if(FAILED(hr=device->CreateCommandQueue(&qdesc,IID_PPV_ARGS(&queue))))
        return fail("CreateCommandQueue",hr);
    ComPtr<ID3D12CommandAllocator> allocator;
    if(FAILED(hr=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))))
        return fail("CreateCommandAllocator",hr);
    ComPtr<ID3D12GraphicsCommandList> list;
    if(FAILED(hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list))))
        return fail("CreateCommandList",hr);
    if(FAILED(hr=list->Close()))return fail("Close",hr);
    ID3D12CommandList* lists[]={list.Get()};
    queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> fence;
    if(FAILED(hr=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))
        return fail("CreateFence",hr);
    if(FAILED(hr=queue->Signal(fence.Get(),1)))return fail("Signal",hr);
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return fail("CreateEventW",HRESULT_FROM_WIN32(GetLastError()));
    const HRESULT wait_hr=fence->SetEventOnCompletion(1,event);
    const DWORD waited=SUCCEEDED(wait_hr)?WaitForSingleObject(event,10000):WAIT_FAILED;
    CloseHandle(event);
    if(FAILED(wait_hr))return fail("SetEventOnCompletion",wait_hr);
    if(waited!=WAIT_OBJECT_0){
        std::cerr << "WARP queue did not complete within 10 seconds\n";
        return 1;
    }
    if(FAILED(hr=device->GetDeviceRemovedReason()))return fail("DeviceRemoved",hr);
    std::cout << "{\"kind\":\"native-d3d12-warp-smoke\","
              << "\"queue_executed\":true,\"fence_completed\":true,"
              << "\"wave_info_available\":"<<(wave_info?"true":"false")
              << ",\"wave_ops\":"<<(wave_info&&options.WaveOps?"true":"false")
              << ",\"min_wave_lanes\":"<<(wave_info?options.WaveLaneCountMin:0)
              << ",\"max_wave_lanes\":"<<(wave_info?options.WaveLaneCountMax:0)
              << ",\"gpu_shader_tested\":false,\"xbox_title_tested\":false}\n";
    return 0;
}
