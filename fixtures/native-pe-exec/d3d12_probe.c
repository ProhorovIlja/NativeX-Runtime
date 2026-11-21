// Original CRT-free Windows x64 Direct3D 12 probe executed through a native PE loader.
// Uses desktop DXGI/D3D12 WARP, NOT Xbox D3D12.X or RDNA2 microcode.
#define WIN32_LEAN_AND_MEAN
#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <dxgi1_4.h>
#include <d3d12.h>

int WINAPI ProbeEntry(void) {
    int result=45;
    HRESULT hr;
    IDXGIFactory4* factory=NULL;
    IDXGIAdapter* warp=NULL;
    ID3D12Device* device=NULL;
    ID3D12CommandQueue* queue=NULL;
    ID3D12Fence* fence=NULL;
    HANDLE event=NULL;
    D3D12_COMMAND_QUEUE_DESC descriptor={0};

    hr=CreateDXGIFactory2(0,&IID_IDXGIFactory4,(void**)&factory);
    if(FAILED(hr)){result=101;goto cleanup;}
    hr=IDXGIFactory4_EnumWarpAdapter(factory,&IID_IDXGIAdapter,(void**)&warp);
    if(FAILED(hr)){result=102;goto cleanup;}
    hr=D3D12CreateDevice((IUnknown*)warp,D3D_FEATURE_LEVEL_11_0,
                         &IID_ID3D12Device,(void**)&device);
    if(FAILED(hr)){result=103;goto cleanup;}
    descriptor.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr=ID3D12Device_CreateCommandQueue(device,&descriptor,&IID_ID3D12CommandQueue,
                                       (void**)&queue);
    if(FAILED(hr)){result=104;goto cleanup;}
    hr=ID3D12Device_CreateFence(device,0,D3D12_FENCE_FLAG_NONE,&IID_ID3D12Fence,
                               (void**)&fence);
    if(FAILED(hr)){result=105;goto cleanup;}
    hr=ID3D12CommandQueue_Signal(queue,fence,1);
    if(FAILED(hr)){result=106;goto cleanup;}
    event=CreateEventW(NULL,FALSE,FALSE,NULL);
    if(event==NULL){result=107;goto cleanup;}
    hr=ID3D12Fence_SetEventOnCompletion(fence,1,event);
    if(FAILED(hr)){result=108;goto cleanup;}
    if(WaitForSingleObject(event,5000)!=WAIT_OBJECT_0){result=109;goto cleanup;}
    hr=ID3D12Device_GetDeviceRemovedReason(device);
    if(FAILED(hr)){result=110;goto cleanup;}

cleanup:
    if(event!=NULL)CloseHandle(event);
    if(fence!=NULL)ID3D12Fence_Release(fence);
    if(queue!=NULL)ID3D12CommandQueue_Release(queue);
    if(device!=NULL)ID3D12Device_Release(device);
    if(warp!=NULL)IDXGIAdapter_Release(warp);
    if(factory!=NULL)IDXGIFactory4_Release(factory);
    return result;
}
