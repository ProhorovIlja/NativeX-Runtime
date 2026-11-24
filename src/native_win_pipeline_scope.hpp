// Local Windows identity discovery after D3D12 device and compiler setup.
#pragma once
#include "native_pipeline_scope.hpp"
struct IDXGIAdapter1;
struct ID3D12Device;
namespace scarlett::pipeline {
// Empty if required identity cannot be established. The compiler module must
// already be loaded. Version metadata is not a cryptographic fingerprint.
Bytes windows_scope(IDXGIAdapter1* adapter,ID3D12Device* device,
                    const wchar_t* compiler_module,const Bytes& producer);
}
