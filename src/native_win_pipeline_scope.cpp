#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "native_win_pipeline_scope.hpp"
#include <winver.h>
namespace scarlett::pipeline {
namespace {
std::uint64_t version(HMODULE module){
    if(!module)return 0;
    std::vector<wchar_t> path(32768);
    const auto n=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    if(!n||n>=path.size())return 0;
    DWORD unused=0;const auto bytes=GetFileVersionInfoSizeW(path.data(),&unused);
    if(!bytes||bytes>4u*1024u*1024u)return 0;
    Bytes data(bytes);
    if(!GetFileVersionInfoW(path.data(),0,bytes,data.data()))return 0;
    void* raw=nullptr;UINT length=0;
    if(!VerQueryValueW(data.data(),L"\\",&raw,&length)||!raw||length<sizeof(VS_FIXEDFILEINFO))return 0;
    const auto* info=static_cast<const VS_FIXEDFILEINFO*>(raw);
    if(info->dwSignature!=0xfeef04bdu)return 0;
    return (static_cast<std::uint64_t>(info->dwFileVersionMS)<<32)|info->dwFileVersionLS;
}
}
Bytes windows_scope(IDXGIAdapter1* adapter,ID3D12Device* device,
                    const wchar_t* compiler_module,const Bytes& producer){
    if(!adapter||!device||!compiler_module||!*compiler_module)return {};
    DXGI_ADAPTER_DESC1 ad{};LARGE_INTEGER driver{};
    if(FAILED(adapter->GetDesc1(&ad))||
       FAILED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),&driver)))return {};
    const auto luid=device->GetAdapterLuid();
    if(luid.LowPart!=ad.AdapterLuid.LowPart||luid.HighPart!=ad.AdapterLuid.HighPart)return {};
    ScopeIdentity x{};x.vendor=ad.VendorId;x.device=ad.DeviceId;x.subsystem=ad.SubSysId;x.revision=ad.Revision;
    x.luid_low=luid.LowPart;x.luid_high=static_cast<std::uint32_t>(luid.HighPart);x.node_count=device->GetNodeCount();
    x.software=(ad.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)!=0;
    x.driver=static_cast<std::uint64_t>(driver.QuadPart);
    x.d3d12=version(GetModuleHandleW(L"d3d12.dll"));x.dxgi=version(GetModuleHandleW(L"dxgi.dll"));
    x.compiler=version(GetModuleHandleW(compiler_module));
    const auto core=GetModuleHandleW(L"D3D12Core.dll");x.core_loaded=core!=nullptr;x.core=version(core);
    x.producer=producer;return compatibility_scope(x);
}
}
