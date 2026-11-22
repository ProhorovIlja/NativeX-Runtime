// Native Windows D3D12 WARP draw-to-DXGI flip-chain and HWND Present probe.
// No game shaders, Xbox commands, CPU emulation or proprietary SDK.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include <iostream>
#include "native_input_core.hpp"
#include "native_frame_timeline.hpp"
using Microsoft::WRL::ComPtr;
namespace {
constexpr UINT Width=64,Height=64;
constexpr wchar_t ClassName[]=L"ScarlettNativeDxgiPresentFixture";
int fail(const char* label,HRESULT hr){
    std::cerr<<label<<" HRESULT=0x"<<std::hex
             <<static_cast<unsigned long>(hr)<<'\n';
    return 1;
}
int skip(const char* label,HRESULT hr){
    std::cout<<"SKIP: "<<label<<" HRESULT=0x"<<std::hex
             <<static_cast<unsigned long>(hr)<<'\n';
    return 77;
}
struct Window {
    HINSTANCE instance=GetModuleHandleW(nullptr);
    HWND hwnd=nullptr;
    bool registered=false;
    ~Window(){
        if(hwnd)DestroyWindow(hwnd);
        if(registered)UnregisterClassW(ClassName,instance);
    }
    bool create(){
        WNDCLASSW cls{};
        cls.lpfnWndProc=DefWindowProcW;
        cls.hInstance=instance;
        cls.lpszClassName=ClassName;
        registered=RegisterClassW(&cls)!=0;
        if(!registered)return false;
        hwnd=CreateWindowExW(0,ClassName,L"Scarlett Native DXGI Present Fixture",
                             WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
                             320,240,nullptr,nullptr,instance,nullptr);
        return hwnd!=nullptr;
    }
};
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
int run(bool latency_one){
    Window window{};
    if(!window.create())
        return skip("Win32 desktop HWND not available",HRESULT_FROM_WIN32(GetLastError()));
    // Synthetic input is passed through the real, independently tested
    // per-device state router. No fake WM_INPUT or virtual controller.
    scarlett::input::Router input{};
    const auto input_ticks=scarlett::timing::monotonic_ns();
    if(!input_ticks){std::cerr<<"monotonic clock unavailable\n";return 1;}
    input.key(0xA11u,0x1eu,false,false,true,input_ticks);
    input.mouse_button(0xB22u,0u,true,input_ticks);
    const auto captured=input.snapshot(input_ticks);
    if(!captured.key_down(0x1eu) || !captured.button_down(0u) ||
       !captured.focused || captured.dropped_events!=0) {
        std::cerr<<"captured native input state mismatch\n";return 1;
    }
    input.key(0xA11u,0x1eu,false,false,false,input_ticks);
    input.mouse_button(0xB22u,0u,false,input_ticks);
    if(!captured.key_down(0x1eu) || input.snapshot(input_ticks).key_down(0x1eu)){
        std::cerr<<"immutable frame input invariant violated\n";return 1;
    }
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr=CreateDXGIFactory2(0,IID_PPV_ARGS(&factory));
    if(FAILED(hr))return skip("DXGI not available",hr);
    ComPtr<IDXGIAdapter> warp;
    hr=factory->EnumWarpAdapter(IID_PPV_ARGS(&warp));
    if(FAILED(hr))return skip("DXGI WARP adapter absent",hr);
    ComPtr<ID3D12Device> device;
    hr=D3D12CreateDevice(warp.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&device));
    if(FAILED(hr))return skip("D3D12 WARP device unsupported",hr);
    D3D12_COMMAND_QUEUE_DESC queue_desc{};
    queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    ComPtr<ID3D12CommandQueue> queue;
    if(FAILED(hr=device->CreateCommandQueue(&queue_desc,IID_PPV_ARGS(&queue))))
        return fail("CreateCommandQueue",hr);
    DXGI_SWAP_CHAIN_DESC1 chain_desc{};
    chain_desc.Width=Width;
    chain_desc.Height=Height;
    chain_desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    chain_desc.Stereo=FALSE;
    chain_desc.SampleDesc.Count=1;
    chain_desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    chain_desc.BufferCount=2;
    chain_desc.Scaling=DXGI_SCALING_STRETCH;
    chain_desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    chain_desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
    if(latency_one) chain_desc.Flags=DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    ComPtr<IDXGISwapChain1> initial;
    hr=factory->CreateSwapChainForHwnd(queue.Get(),window.hwnd,
                                        &chain_desc,nullptr,nullptr,&initial);
    if(FAILED(hr)){
        if(hr==DXGI_ERROR_UNSUPPORTED || hr==DXGI_ERROR_NOT_CURRENTLY_AVAILABLE ||
           hr==DXGI_ERROR_INVALID_CALL || hr==E_ACCESSDENIED)
            return skip("DXGI HWND flip-chain unavailable on runner",hr);
        return fail("CreateSwapChainForHwnd",hr);
    }
    ComPtr<IDXGISwapChain3> chain;
    if(FAILED(hr=initial.As(&chain)))return fail("Query IDXGISwapChain3",hr);
    if(latency_one){
        ComPtr<IDXGISwapChain2> latency_chain;
        if(FAILED(hr=initial.As(&latency_chain)))
            return skip("IDXGISwapChain2 latency interface unavailable",hr);
        hr=latency_chain->SetMaximumFrameLatency(1);
        if(FAILED(hr))
            return skip("SetMaximumFrameLatency(1) unsupported",hr);
        HANDLE ready=latency_chain->GetFrameLatencyWaitableObject();
        if(!ready)
            return skip("DXGI frame latency waitable object unavailable",
                        HRESULT_FROM_WIN32(GetLastError()));
        const DWORD waited=WaitForSingleObject(ready,5000);
        CloseHandle(ready);
        if(waited!=WAIT_OBJECT_0) {
            std::cerr<<"Frame latency waitable object not signaled\n";return 1;
        }
    }
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.NumDescriptors=2;
    ComPtr<ID3D12DescriptorHeap> rtv_heap;
    if(FAILED(hr=device->CreateDescriptorHeap(&heap_desc,IID_PPV_ARGS(&rtv_heap))))
        return fail("CreateDescriptorHeap",hr);
    ComPtr<ID3D12Resource> buffers[2];
    const UINT stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtv=rtv_heap->GetCPUDescriptorHandleForHeapStart();
    for(UINT i=0;i<2;i++){
        if(FAILED(hr=chain->GetBuffer(i,IID_PPV_ARGS(&buffers[i]))))
            return fail("IDXGISwapChain::GetBuffer",hr);
        D3D12_CPU_DESCRIPTOR_HANDLE dest=rtv;
        dest.ptr+=static_cast<SIZE_T>(i)*stride;
        device->CreateRenderTargetView(buffers[i].Get(),nullptr,dest);
    }
    const UINT index=chain->GetCurrentBackBufferIndex();
    if(index>=2){std::cerr<<"invalid flip-chain buffer index\n";return 1;}
    ComPtr<ID3D12CommandAllocator> allocator;
    if(FAILED(hr=device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&allocator))))
        return fail("CreateCommandAllocator",hr);
    ComPtr<ID3D12GraphicsCommandList> list;
    if(FAILED(hr=device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,
                      allocator.Get(),nullptr,IID_PPV_ARGS(&list))))
        return fail("CreateCommandList",hr);
    auto before=transition(buffers[index].Get(),D3D12_RESOURCE_STATE_PRESENT,
                            D3D12_RESOURCE_STATE_RENDER_TARGET);
    list->ResourceBarrier(1,&before);
    D3D12_CPU_DESCRIPTOR_HANDLE dest=rtv;
    dest.ptr+=static_cast<SIZE_T>(index)*stride;
    constexpr float clear[]={0.125f,0.25f,0.875f,1.0f};
    list->ClearRenderTargetView(dest,clear,0,nullptr);
    auto after=transition(buffers[index].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,
                           D3D12_RESOURCE_STATE_PRESENT);
    list->ResourceBarrier(1,&after);
    if(FAILED(hr=list->Close()))return fail("Close command list",hr);
    const auto simulation_begin_ns=scarlett::timing::monotonic_ns();
    if(!captured.key_down(0x1eu)){std::cerr<<"simulation input lost\n";return 1;}
    const auto simulation_end_ns=scarlett::timing::monotonic_ns();
    ID3D12CommandList* batch[]{list.Get()};
    queue->ExecuteCommandLists(1,batch);
    const auto render_submitted_ns=scarlett::timing::monotonic_ns();
    hr=chain->Present(1,0);
    const auto present_return_ns=scarlett::timing::monotonic_ns();
    if(FAILED(hr)){
        if(hr==DXGI_ERROR_DEVICE_REMOVED || hr==DXGI_ERROR_DEVICE_RESET)
            return fail("Present device lost",hr);
        return skip("DXGI Present unavailable in session",hr);
    }
    ComPtr<ID3D12Fence> fence;
    if(FAILED(hr=device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence))))
        return fail("CreateFence",hr);
    if(FAILED(hr=queue->Signal(fence.Get(),1)))return fail("Signal",hr);
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    if(!event)return fail("CreateEventW",HRESULT_FROM_WIN32(GetLastError()));
    hr=fence->SetEventOnCompletion(1,event);
    const DWORD wait=SUCCEEDED(hr)?WaitForSingleObject(event,10000):WAIT_FAILED;
    CloseHandle(event);
    if(FAILED(hr))return fail("SetEventOnCompletion",hr);
    if(wait!=WAIT_OBJECT_0){std::cerr<<"queue fence wait timeout\n";return 1;}
    if(FAILED(hr=device->GetDeviceRemovedReason()))
        return fail("D3D12 WARP removed",hr);
    // The timestamp is the CPU return from Present, not monitor scanout.
    scarlett::timing::Timeline profile{};
    if(!profile.record({input_ticks,simulation_begin_ns,simulation_end_ns,
                        render_submitted_ns,present_return_ns})){
        std::cerr<<"invalid frame timing phase order\n";return 1;
    }
    const auto summary=profile.summarize();
    if(summary.cpu_frame.count!=1 || summary.input_to_present_return.count!=1 ||
       summary.rejected!=0 || !summary.cpu_frame.maximum_ns){
        std::cerr<<"native frame telemetry invariant failed\n";return 1;
    }
    std::cout<<"{\"kind\":\"native-win-input-frame-to-dxgi-present\","
             "\"source\":\"original synthetic controls\","
             "\"input_snapshot_immutable\":true,"
             "\"dxgi_present_completed\":true,"
             "\"frame_phase_profiled\":true,"
             "\"cpu_input_to_present_return_ns\":"<<summary.input_to_present_return.maximum_ns<<","
             "\"cpu_frame_ns\":"<<summary.cpu_frame.maximum_ns<<","
             "\"real_input_hardware_tested\":false,"
             "\"physical_gpu_or_monitor_tested\":false,"
             "\"input_to_photon_tested\":false,"
             "\"game_tested\":false}\n";
    return 0;
}
}
int main(int argc,char** /*argv*/){
    if(argc==1)return run(false);
    std::cerr<<"Usage: native_win_input_to_present\n";return 2;
}
