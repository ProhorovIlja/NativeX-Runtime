// Original Windows x64 WASAPI endpoint and shared-mode mix-format probe.
// No synthesized audio playback, no recording, no proprietary game codecs.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <objbase.h>
#include <cstdint>
#include <iostream>
#include <string>
namespace {
template<typename T> struct ComObject {
    T* ptr=nullptr;
    ~ComObject(){if(ptr)ptr->Release();}
    ComObject()=default;
    ComObject(const ComObject&)=delete;
    ComObject& operator=(const ComObject&)=delete;
    T** out(){return &ptr;}
    T* operator->()const{return ptr;}
};
struct Apartment {
    HRESULT code;
    Apartment():code(CoInitializeEx(nullptr,COINIT_MULTITHREADED)){}
    ~Apartment(){if(SUCCEEDED(code))CoUninitialize();}
};
int fail(const char* phase,HRESULT code){
    std::cerr<<"WASAPI "<<phase<<" HRESULT="<<std::hex
             <<static_cast<unsigned long>(code)<<'\n';
    return 1;
}
int missing(const char* phase,HRESULT code){
    std::cout<<"SKIP: Windows audio endpoint not available: "<<phase
             <<" HRESULT="<<std::hex<<static_cast<unsigned long>(code)<<'\n';
    return 77;
}
int run(){
    Apartment apartment{};
    if(FAILED(apartment.code))return fail("CoInitializeEx",apartment.code);
    ComObject<IMMDeviceEnumerator> enumerator{};
    HRESULT hr=CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,
                    CLSCTX_INPROC_SERVER,IID_PPV_ARGS(enumerator.out()));
    if(FAILED(hr))return missing("MMDeviceEnumerator",hr);
    ComObject<IMMDevice> endpoint{};
    hr=enumerator->GetDefaultAudioEndpoint(eRender,eConsole,endpoint.out());
    if(FAILED(hr))return missing("GetDefaultAudioEndpoint",hr);
    ComObject<IAudioClient> client{};
    hr=endpoint->Activate(__uuidof(IAudioClient),CLSCTX_INPROC_SERVER,
                          nullptr,reinterpret_cast<void**>(client.out()));
    if(FAILED(hr))return missing("Activate IAudioClient",hr);
    WAVEFORMATEX* format=nullptr;
    hr=client->GetMixFormat(&format);
    if(FAILED(hr))return fail("GetMixFormat",hr);
    const std::uint32_t channels=format?format->nChannels:0;
    const std::uint32_t rate=format?format->nSamplesPerSec:0;
    const std::uint32_t bits=format?format->wBitsPerSample:0;
    const std::uint32_t block_align=format?format->nBlockAlign:0;
    const std::uint32_t tag=format?format->wFormatTag:0;
    CoTaskMemFree(format);
    if(!channels||!rate||!bits||!block_align)
        return fail("invalid mix format",E_INVALIDARG);
    REFERENCE_TIME default_period=0,min_period=0;
    hr=client->GetDevicePeriod(&default_period,&min_period);
    if(FAILED(hr))return fail("GetDevicePeriod",hr);
    if(default_period<=0 || min_period<=0)
        return fail("invalid audio periods",E_INVALIDARG);
    // Does not initialize or start an audio stream; capability probe only.
    std::cout<<"{\"kind\":\"native-windows-wasapi-shared-mix-format\","
             "\"render_endpoint_found\":true,"
             "\"iaudioclient_activated\":true,"
             "\"mix_format_tag\":"<<tag<<","
             "\"sample_rate_hz\":"<<rate<<","
             "\"channels\":"<<channels<<","
             "\"bits_per_sample\":"<<bits<<","
             "\"block_align_bytes\":"<<block_align<<","
             "\"default_period_100ns\":"<<default_period<<","
             "\"minimum_period_100ns\":"<<min_period<<","
             "\"audio_stream_started\":false,"
             "\"actual_audio_device_playback_verified\":false,"
             "\"game_audio_tested\":false}\n";
    return 0;
}
}
int main(){return run();}
