// Native Win32 WARP: multi-frame DXGI Present, one-frame waitable pacing,
// safe backbuffer reuse, ResizeBuffers and actual fence completion.
// Original synthetic graphics work only; no title DX12.X or real monitor proof.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include "native_frame_timeline.hpp"
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT TotalFrames=12;
constexpr UINT ResizeAt=6;
constexpr wchar_t ClassName[]=L"ScarlettNativeMultiframeDxgi";
struct Window {
    HINSTANCE module=GetModuleHandleW(nullptr);
    HWND hwnd=nullptr;
    bool registered=false;
    ~Window(){
        if(hwnd)DestroyWindow(hwnd);
        if(registered)UnregisterClassW(ClassName,module);
    }
    bool create(){
        WNDCLASSW cls{};
        cls.lpfnWndProc=DefWindowProcW;
        cls.hInstance=module;
        cls.lpszClassName=ClassName;
        registered=RegisterClassW(&cls)!=0;
        if(!registered)return false;
        hwnd=CreateWindowExW(0,ClassName,L"Native D3D12 MultiFrame Fixture",
                             WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
                             320,240,nullptr,nullptr,module,nullptr);
        return hwnd!=nullptr;
    }
};
struct Handle {
    HANDLE handle=nullptr;
    ~Handle(){if(handle)CloseHandle(handle);}
};
int fail(const char* what,HRESULT hr){
    std::cerr<<what<<" hr=0x"<<std::hex
             <<static_cast<unsigned long>(hr)<<'\n';
    return 1;
}
int skip(const char* why,HRESULT hr){
    std::cout<<"SKIP: "<<why<<" hr=0x"<<std::hex
             <<static_cast<unsigned long>(hr)<<'\n';
    return 77;
}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* resource,
                                   D3D12_RESOURCE_STATES before,
                                   D3D12_RESOURCE_STATES after){
    D3D12_RESOURCE_BARRIER b{};
    b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource=resource;
    b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore=before;
    b.Transition.StateAfter=after;
    return b;
}
int run(){
    Window window{};
    if(!window.create())
        return skip("Win32 desktop window not available",
                    HRESULT_FROM_WIN32(GetLastError()));
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr=CreateDXGIFactory2(0,IID_PPV_ARGS(&factory));
    if(FAILED(hr))return skip("DXGI factory unavailable",hr);
    ComPtr<IDXGIAdapter> warp;
    hr=factory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
    if(FAILED(hr))return skip("WARP adapter unavailable",hr);
    ComPtr<ID3D12Device> device;
    hr=D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
    if(FAILED(hr))return skip("D3D12 WARP device unavailable",hr);
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if(FAILED(hr=device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue))))
        return fail("CreateCommandQueue",hr);
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width=64;desc.Height=64;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count=1;
    desc.BufferCount=2;
    desc.Scaling=DXGI_SCALING_STRETCH;
    desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
    desc.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    ComPtr<IDXGISwapChain1> initial;
    hr=factory->CreateSwapChainForHwnd(queue.Get(),window.hwnd,
                                        &desc,nullptr,nullptr,&initial);
    if(FAILED(hr)) {
        if(hr==DXGI_ERROR_UNSUPPORTED || hr==DXGI_ERROR_NOT_CURRENTLY_AVAILABLE ||
           hr==DXGI_ERROR_INVALID_CALL || hr==E_ACCESSDENIED)
            return skip("Win32 flip-chain unavailable in session",hr);
        return fail("CreateSwapChainForHwnd",hr);
    }
    ComPtr<IDXGISwapChain3> chain;
    if(FAILED(hr=initial.As(&chain)))return fail("IDXGISwapChain3",hr);
    ComPtr<IDXGISwapChain2> pacing;
    if(FAILED(hr=initial.As(&pacing)))
        return skip("frame latency interface missing",hr);
    hr=pacing->SetMaximumFrameLatency(1);
    if(FAILED(hr))return skip("one-frame waitable presentation unavailable",hr);
    Handle latency{};
    latency.handle=pacing->GetFrameLatencyWaitableObject();
    if(!latency.handle)
        return skip("DXGI frame latency waitable handle missing",
                    HRESULT_FROM_WIN32(GetLastError()));
    ComPtr<ID3D12Fence> fence;
    if(FAILED(hr=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))
        return fail("CreateFence",hr);
    Handle completed{};
    completed.handle=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!completed.handle)return fail("CreateEventW",HRESULT_FROM_WIN32(GetLastError()));
    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc{};
    rtv_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.NumDescriptors=1;
    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    if(FAILED(hr=device->CreateDescriptorHeap(&rtv_desc,IID_PPV_ARGS(&rtv_heap))))
        return fail("CreateDescriptorHeap",hr);
    scarlett::timing::Timeline timeline{};
    UINT presented=0;
    UINT swaps=0;
    UINT indices_seen=0;
    for(UINT frame=0;frame<TotalFrames;frame++){
        if(frame==ResizeAt){
            rtv_heap.Reset();
            hr=chain->ResizeBuffers(2,96,64,DXGI_FORMAT_UNKNOWN,
                                    DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
            if(FAILED(hr))return fail("ResizeBuffers after queued frames completed",hr);
            if(FAILED(hr=device->CreateDescriptorHeap(&rtv_desc,IID_PPV_ARGS(&rtv_heap))))
                return fail("CreateDescriptorHeap after ResizeBuffers",hr);
            DXGI_SWAP_CHAIN_DESC1 actual{};
            if(FAILED(hr=chain->GetDesc1(&actual)))return fail("GetDesc1",hr);
            if(actual.Width!=96 || actual.Height!=64 || actual.BufferCount!=2){
                std::cerr<<"ResizeBuffers descriptor did not update\n";
                return 1;
            }
            ++swaps;
        }
        const DWORD available=WaitForSingleObject(latency.handle,5000);
        if(available!=WAIT_OBJECT_0){
            std::cerr<<"DXGI frame waitable object not signaled\n";return 1;
        }
        const std::uint64_t begin=scarlett::timing::monotonic_ns();
        if(!begin)return 1;
        const UINT buffer_index=chain->GetCurrentBackBufferIndex();
        if(buffer_index>=2){std::cerr<<"invalid backbuffer index\n";return 1;}
        indices_seen|=(1u<<buffer_index);
        // Each list/allocator and its backbuffer COM reference are released
        // at end of the frame AFTER its completion fence, before a resize.
        {
            ComPtr<ID3D12Resource> buffer;
            if(FAILED(hr=chain->GetBuffer(buffer_index,IID_PPV_ARGS(&buffer))))
                return fail("IDXGISwapChain::GetBuffer",hr);
            device->CreateRenderTargetView(buffer.Get(),nullptr,
                 rtv_heap->GetCPUDescriptorHandleForHeapStart());
            ComPtr<ID3D12CommandAllocator> allocator;
            if(FAILED(hr=device->CreateCommandAllocator(
                          D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))))
                return fail("CreateCommandAllocator",hr);
            ComPtr<ID3D12GraphicsCommandList> commands;
            if(FAILED(hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
                       allocator.Get(),nullptr,IID_PPV_ARGS(&commands))))
                return fail("CreateCommandList",hr);
            const auto sim_end=scarlett::timing::monotonic_ns();
            auto from_present=transition(buffer.Get(),D3D12_RESOURCE_STATE_PRESENT,
                                           D3D12_RESOURCE_STATE_RENDER_TARGET);
            commands->ResourceBarrier(1,&from_present);
            const float clear[4]={
                static_cast<float>(frame+1u)/16.0f,
                static_cast<float>((frame%4u)+1u)/5.0f,0.25f,1.0f
            };
            commands->ClearRenderTargetView(
                rtv_heap->GetCPUDescriptorHandleForHeapStart(),clear,0,nullptr);
            auto to_present=transition(buffer.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,
                                        D3D12_RESOURCE_STATE_PRESENT);
            commands->ResourceBarrier(1,&to_present);
            if(FAILED(hr=commands->Close()))return fail("Close command list",hr);
            ID3D12CommandList* one[]={commands.Get()};
            queue->ExecuteCommandLists(1,one);
            const auto submitted=scarlett::timing::monotonic_ns();
            hr=chain->Present(1,0);
            const auto returned=scarlett::timing::monotonic_ns();
            if(FAILED(hr))return fail("DXGI Present",hr);
            if(!timeline.record({0,begin,sim_end,submitted,returned})){
                std::cerr<<"frame timestamps invalid\n";return 1;
            }
            ++presented;
            const UINT64 value=static_cast<UINT64>(frame)+1ull;
            if(FAILED(hr=queue->Signal(fence.Get(),value)))
                return fail("D3D12 fence signal",hr);
            if(FAILED(hr=fence->SetEventOnCompletion(value,completed.handle)))
                return fail("SetEventOnCompletion",hr);
            const DWORD finished=WaitForSingleObject(completed.handle,10000);
            if(finished!=WAIT_OBJECT_0){
                std::cerr<<"frame GPU fence timeout\n";return 1;
            }
            if(FAILED(hr=device->GetDeviceRemovedReason()))
                return fail("device removed during frame loop",hr);
        }
    }
    const auto metrics=timeline.summarize();
    if(presented!=TotalFrames || swaps!=1 || indices_seen!=3u ||
       metrics.cpu_frame.count!=TotalFrames || metrics.rejected!=0 ||
       metrics.cpu_frame.p95_ns==0){
        std::cerr<<"multi-frame/resizing/stats invariants failed\n";return 1;
    }
    std::cout<<"{\"kind\":\"native-win-warp-multiframe-dxgi\","
             "\"frames_presented\":"<<presented<<","
             "\"resize_buffers_succeeded\":"<<swaps<<","
             "\"both_backbuffer_indices_seen\":true,"
             "\"per_frame_fences_complete\":true,"
             "\"frame_latency_limit\":1,"
             "\"cpu_frame_p95_ns\":"<<metrics.cpu_frame.p95_ns<<","
             "\"physical_gpu_verified\":false,"
             "\"human_visible_pixels_verified\":false,"
             "\"game_tested\":false}\n";
    return 0;
}
}
int main(){return run();}
