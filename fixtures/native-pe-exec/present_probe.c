// Original CRT-free Win32 PE32+ HWND DXGI Present through native loader.
// Separately source-built by MSVC; no Xbox title or emulated CPU.
#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4201)
#endif
#include <windows.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

static LRESULT CALLBACK ProbeWindowProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    return DefWindowProcW(h,msg,w,l);
}
int WINAPI ProbeEntry(void){
    int result=46;
    HRESULT hr=S_OK;
    HINSTANCE instance=GetModuleHandleW(NULL);
    const wchar_t* klass=L"ScarlettNativeLoadedDxgiWindow";
    WNDCLASSW wc={0};
    BOOL registered=FALSE;
    HWND window=NULL;
    IDXGIFactory4* factory=NULL;
    IDXGIAdapter* warp=NULL;
    ID3D12Device* device=NULL;
    ID3D12CommandQueue* queue=NULL;
    IDXGISwapChain1* swap=NULL;
    ID3D12Fence* fence=NULL;
    HANDLE event=NULL;
    D3D12_COMMAND_QUEUE_DESC queue_desc={0};
    DXGI_SWAP_CHAIN_DESC1 swap_desc={0};

    if(instance==NULL){result=111;goto cleanup;}
    wc.lpfnWndProc=ProbeWindowProc;
    wc.hInstance=instance;
    wc.lpszClassName=klass;
    if(!RegisterClassW(&wc)){result=112;goto cleanup;}
    registered=TRUE;
    window=CreateWindowExW(0,klass,L"Scarlett Native PE DXGI Present",
                           WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,
                           320,240,NULL,NULL,instance,NULL);
    if(!window){result=113;goto cleanup;}
    hr=CreateDXGIFactory2(0,&IID_IDXGIFactory4,(void**)&factory);
    if(FAILED(hr)){result=114;goto cleanup;}
    hr=IDXGIFactory4_EnumWarpAdapter(factory,&IID_IDXGIAdapter,(void**)&warp);
    if(FAILED(hr)){result=115;goto cleanup;}
    hr=D3D12CreateDevice((IUnknown*)warp,D3D_FEATURE_LEVEL_11_0,
                         &IID_ID3D12Device,(void**)&device);
    if(FAILED(hr)){result=116;goto cleanup;}
    queue_desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr=ID3D12Device_CreateCommandQueue(device,&queue_desc,&IID_ID3D12CommandQueue,
                                       (void**)&queue);
    if(FAILED(hr)){result=117;goto cleanup;}
    swap_desc.Width=64;
    swap_desc.Height=64;
    swap_desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_desc.SampleDesc.Count=1;
    swap_desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_desc.BufferCount=2;
    swap_desc.Scaling=DXGI_SCALING_STRETCH;
    swap_desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_desc.AlphaMode=DXGI_ALPHA_MODE_IGNORE;
    hr=IDXGIFactory4_CreateSwapChainForHwnd(
        factory,(IUnknown*)queue,window,&swap_desc,NULL,NULL,&swap);
    if(FAILED(hr)){result=118;goto cleanup;}
    // This checks the actual flip-chain Present API from a manually mapped PE.
    // No rendered pixel content is asserted; native WARP clear/readback
    // is tested separately by the normal D3D12 executable fixtures.
    hr=IDXGISwapChain1_Present(swap,1,0);
    if(FAILED(hr)){result=119;goto cleanup;}
    hr=ID3D12Device_CreateFence(device,0,D3D12_FENCE_FLAG_NONE,
                               &IID_ID3D12Fence,(void**)&fence);
    if(FAILED(hr)){result=120;goto cleanup;}
    hr=ID3D12CommandQueue_Signal(queue,fence,1);
    if(FAILED(hr)){result=121;goto cleanup;}
    event=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(event==NULL){result=122;goto cleanup;}
    hr=ID3D12Fence_SetEventOnCompletion(fence,1,event);
    if(FAILED(hr)){result=123;goto cleanup;}
    if(WaitForSingleObject(event,7000)!=WAIT_OBJECT_0){result=124;goto cleanup;}
    hr=ID3D12Device_GetDeviceRemovedReason(device);
    if(FAILED(hr)){result=125;goto cleanup;}

cleanup:
    if(event!=NULL)CloseHandle(event);
    if(fence!=NULL)ID3D12Fence_Release(fence);
    if(swap!=NULL)IDXGISwapChain1_Release(swap);
    if(queue!=NULL)ID3D12CommandQueue_Release(queue);
    if(device!=NULL)ID3D12Device_Release(device);
    if(warp!=NULL)IDXGIAdapter_Release(warp);
    if(factory!=NULL)IDXGIFactory4_Release(factory);
    if(window!=NULL)DestroyWindow(window);
    if(registered)UnregisterClassW(klass,instance);
    return result;
}
