// Source-built Windows x64 interface/contract test for Microsoft GameInput v4.
// The included Microsoft headers are pinned MIT-licensed official sources.
// No game assets, controller emulation or automatic haptic/trigger commands.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GameInput.h>
#include <DualSenseHelper.h>
#include <cstdint>
#include <cstring>
#include <iostream>

namespace {
using GameInputInit = HRESULT (WINAPI*)(REFIID,void**);

int run() {
    using GameInput::DualSense::Trigger;
    const bool sony_id_test=
        GameInput::DualSense::IsDualSense(0x054c,0x0ce6) &&
        GameInput::DualSense::IsDualSense(0x054c,0x0df2) &&
        !GameInput::DualSense::IsDualSense(0x045e,0x0ce6) &&
        !GameInput::DualSense::IsDualSense(0x054c,0xffff);
    // Invalid parameters must fail before dereferencing a NULL device.
    // Never call output functions with valid parameters without physical hardware.
    const bool invalid_bounds_test=
        GameInput::DualSense::SetDualSenseTriggerOff(
            nullptr,static_cast<Trigger>(99))==E_INVALIDARG &&
        GameInput::DualSense::SetDualSenseTriggerFeedback(
            nullptr,Trigger::Right,10,1)==E_INVALIDARG &&
        GameInput::DualSense::SetDualSenseTriggerFeedback(
            nullptr,Trigger::Left,9,9)==E_INVALIDARG &&
        GameInput::DualSense::SetDualSenseTriggerWeapon(
            nullptr,Trigger::Right,1,8,2)==E_INVALIDARG &&
        GameInput::DualSense::SetDualSenseTriggerWeapon(
            nullptr,Trigger::Left,7,7,2)==E_INVALIDARG &&
        GameInput::DualSense::SetDualSenseTriggerVibration(
            nullptr,Trigger::Right,3,9,40)==E_INVALIDARG &&
        GameInput::DualSense::SetDualSenseTriggerVibration(
            nullptr,Trigger::Right,3,1,256)==E_INVALIDARG;
    if(!sony_id_test || !invalid_bounds_test) {
        std::cerr << "GameInput DualSense source contract validation failed\n";
        return 1;
    }
    // System32-only: never resolve arbitrary DLLs from current directory.
    HMODULE module=LoadLibraryExW(L"GameInputRedist.dll",nullptr,
                                 LOAD_LIBRARY_SEARCH_SYSTEM32);
    bool runtime_loaded=module!=nullptr;
    bool runtime_initialized=false;
    bool has_gamepad_reading=false;
    bool reading_decoded=false;
    bool known_dualsense_reading=false;
    if(module) {
        FARPROC address=GetProcAddress(module,"GameInputInitialize");
        static_assert(sizeof(GameInputInit)==sizeof(FARPROC),
                      "Windows function pointers must have matching size");
        GameInputInit initialize=nullptr;
        if(address)std::memcpy(&initialize,&address,sizeof(initialize));
        if(initialize) {
            GameInput::v4::IGameInput* ctx=nullptr;
            const HRESULT status=initialize(
                GameInput::v4::IID_IGameInput,reinterpret_cast<void**>(&ctx));
            if(SUCCEEDED(status) && ctx) {
                runtime_initialized=true;
                GameInput::v4::IGameInputReading* current=nullptr;
                const HRESULT reading_status=ctx->GetCurrentReading(
                    GameInput::v4::GameInputKindGamepad,nullptr,&current);
                if(SUCCEEDED(reading_status) && current) {
                    has_gamepad_reading=true;
                    GameInput::v4::GameInputGamepadState pad{};
                    reading_decoded=current->GetGamepadState(&pad);
                    // Exercise actual API-v4 device identity method, without
                    // logging device serials or issuing any output report.
                    GameInput::v4::IGameInputDevice* device=nullptr;
                    current->GetDevice(&device);
                    if(device) {
                        known_dualsense_reading=GameInput::DualSense::IsDualSense(
                            device->GetVendorId(),device->GetProductId());
                        device->Release();
                    }
                    current->Release();
                }
                ctx->Release();
            } else if(ctx) {
                ctx->Release();
            }
        }
        FreeLibrary(module);
    }
    std::cout<<"{\"kind\":\"official-gameinput-v4-dualsense-interface\","
             "\"official_sdk_header_compiled\":true,"
             "\"dualsense_id_checks\":true,"
             "\"invalid_trigger_bounds_rejected\":true,"
             "\"runtime_dll_system32_present\":"<<(runtime_loaded?"true":"false")<<','
             "\"runtime_initialized\":"<<(runtime_initialized?"true":"false")<<','
             "\"physical_gamepad_reading\":"<<(has_gamepad_reading?"true":"false")<<','
             "\"gamepad_state_read\":"<<(reading_decoded?"true":"false")<<','
             "\"physical_dualsense_seen\":"<<(known_dualsense_reading?"true":"false")<<','
             "\"raw_trigger_output_sent\":false,"
             "\"bluetooth_haptics_supported_or_tested\":false,"
             "\"game_integration_tested\":false}\n";
    return 0;
}
}
int main() {return run();}
