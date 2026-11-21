// Native Win32 Raw Input and Windows XInput 1.4 integration probe.
// Registers only foreground keyboard/mouse; never hooks or injects controls.
// This is original test code, not a GTA VI controller/game implementation.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include "native_input_core.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <iostream>

namespace {
using scarlett::input::Gamepad;
using scarlett::input::Router;
struct NativeInputProbe {
    Router router{};
    bool synthetic_window_message_seen = false;
    bool interactive = false;
    std::uint64_t raw_messages = 0;
    std::uint64_t rejected_reports = 0;
    std::uint64_t xinput_polls = 0;
    std::uint32_t connected_slots = 0;
    LARGE_INTEGER frequency{};
    alignas(RAWINPUT) std::array<std::uint8_t,65536> raw_buffer{};

    std::uint64_t tick() const {
        LARGE_INTEGER counter{};
        return QueryPerformanceCounter(&counter) ?
            static_cast<std::uint64_t>(counter.QuadPart):0;
    }
    void poll_xinput() {
        if(!router.focused())return;
        ++xinput_polls;
        connected_slots=0;
        for(DWORD i=0;i<XUSER_MAX_COUNT;i++) {
            XINPUT_STATE raw{};
            const DWORD status=XInputGetState(i,&raw);
            Gamepad pad{};
            if(status==ERROR_SUCCESS) {
                ++connected_slots;
                pad.connected=true;
                pad.buttons=raw.Gamepad.wButtons;
                pad.left_x=raw.Gamepad.sThumbLX;
                pad.left_y=raw.Gamepad.sThumbLY;
                pad.right_x=raw.Gamepad.sThumbRX;
                pad.right_y=raw.Gamepad.sThumbRY;
                pad.left_trigger=raw.Gamepad.bLeftTrigger;
                pad.right_trigger=raw.Gamepad.bRightTrigger;
            }else if(status!=ERROR_DEVICE_NOT_CONNECTED) {
                ++rejected_reports;
            }
            router.gamepad(static_cast<std::uint8_t>(i),pad,tick());
        }
    }
    void process_raw(HRAWINPUT handle) {
        UINT size=0;
        if(GetRawInputData(handle,RID_INPUT,nullptr,&size,
                           sizeof(RAWINPUTHEADER))!=0 ||
           size<sizeof(RAWINPUTHEADER) || size>raw_buffer.size()) {
            ++rejected_reports;return;
        }
        UINT copied=size;
        const UINT result=GetRawInputData(handle,RID_INPUT,raw_buffer.data(),
                                           &copied,sizeof(RAWINPUTHEADER));
        if(result==static_cast<UINT>(-1) || result!=size) {
            ++rejected_reports;return;
        }
        const auto* packet=reinterpret_cast<const RAWINPUT*>(raw_buffer.data());
        if(packet->header.dwSize!=size) {++rejected_reports;return;}
        ++raw_messages;
        const auto device=static_cast<std::uint64_t>(
            reinterpret_cast<std::uintptr_t>(packet->header.hDevice));
        const auto time=tick();
        if(packet->header.dwType==RIM_TYPEKEYBOARD) {
            const RAWKEYBOARD& key=packet->data.keyboard;
            // 255 is a keyboard overrun/fake-key identifier.
            if(key.VKey==255u)return;
            router.key(device,key.MakeCode,(key.Flags & RI_KEY_E0)!=0,
                       (key.Flags & RI_KEY_E1)!=0,
                       (key.Flags & RI_KEY_BREAK)==0,time);
        }else if(packet->header.dwType==RIM_TYPEMOUSE) {
            const RAWMOUSE& mouse=packet->data.mouse;
            router.mouse_move(device,mouse.lLastX,mouse.lLastY,
                              (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)!=0,time);
            struct ButtonFlag {USHORT down;USHORT up;};
            constexpr ButtonFlag pairs[]={
                {RI_MOUSE_LEFT_BUTTON_DOWN,RI_MOUSE_LEFT_BUTTON_UP},
                {RI_MOUSE_RIGHT_BUTTON_DOWN,RI_MOUSE_RIGHT_BUTTON_UP},
                {RI_MOUSE_MIDDLE_BUTTON_DOWN,RI_MOUSE_MIDDLE_BUTTON_UP},
                {RI_MOUSE_BUTTON_4_DOWN,RI_MOUSE_BUTTON_4_UP},
                {RI_MOUSE_BUTTON_5_DOWN,RI_MOUSE_BUTTON_5_UP}
            };
            for(std::uint8_t i=0;i<5;i++) {
                if(mouse.usButtonFlags & pairs[i].down)
                    router.mouse_button(device,i,true,time);
                if(mouse.usButtonFlags & pairs[i].up)
                    router.mouse_button(device,i,false,time);
            }
            if(mouse.usButtonFlags & RI_MOUSE_WHEEL)
                router.mouse_wheel(device,static_cast<SHORT>(mouse.usButtonData),
                                   false,time);
            if(mouse.usButtonFlags & RI_MOUSE_HWHEEL)
                router.mouse_wheel(device,static_cast<SHORT>(mouse.usButtonData),
                                   true,time);
        }
    }
};
LRESULT CALLBACK window_proc(HWND hwnd,UINT message,WPARAM w,LPARAM l) {
    if(message==WM_NCCREATE) {
        const auto* info=reinterpret_cast<const CREATESTRUCTW*>(l);
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(info->lpCreateParams));
        return TRUE;
    }
    auto* ctx=reinterpret_cast<NativeInputProbe*>(
        GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(ctx) {
        switch(message) {
            case WM_APP+1:
                ctx->synthetic_window_message_seen=true;
                return 0;
            case WM_ACTIVATEAPP:
                ctx->router.set_focus(w!=FALSE,ctx->tick());
                return 0;
            case WM_INPUT:
                // For RIM_INPUT, DefWindowProc must run to allow Win32 cleanup.
                ctx->process_raw(reinterpret_cast<HRAWINPUT>(l));
                return DefWindowProcW(hwnd,message,w,l);
            case WM_TIMER:
                if(w==1)ctx->poll_xinput();
                return 0;
            default: break;
        }
    }
    if(message==WM_CLOSE) {DestroyWindow(hwnd);return 0;}
    if(message==WM_DESTROY) {PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,message,w,l);
}
int run(bool interactive) {
    NativeInputProbe probe{};
    probe.interactive=interactive;
    if(!QueryPerformanceFrequency(&probe.frequency)) {
        std::cerr<<"QPC frequency unavailable\n";return 1;
    }
    const HINSTANCE instance=GetModuleHandleW(nullptr);
    const wchar_t klass[]=L"ScarlettNativeForegroundInputFixture";
    WNDCLASSW wc{};
    wc.lpfnWndProc=window_proc;
    wc.hInstance=instance;
    wc.lpszClassName=klass;
    if(!RegisterClassW(&wc)) {
        std::cerr<<"SKIP: no interactive Win32 window class\n";return 77;
    }
    const HWND hwnd=CreateWindowExW(0,klass,
        L"Scarlett Native Input Diagnostic (close to exit)",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,640,220,
        nullptr,nullptr,instance,&probe);
    if(!hwnd) {
        std::cerr<<"SKIP: no Win32 desktop window\n";
        UnregisterClassW(klass,instance);return 77;
    }
    RAWINPUTDEVICE devices[2]{};
    devices[0].usUsagePage=0x01; // HID Generic Desktop
    devices[0].usUsage=0x02;     // Mouse
    devices[0].dwFlags=RIDEV_DEVNOTIFY;
    devices[0].hwndTarget=hwnd;
    devices[1].usUsagePage=0x01;
    devices[1].usUsage=0x06;     // Keyboard
    devices[1].dwFlags=RIDEV_DEVNOTIFY;
    devices[1].hwndTarget=hwnd;
    const bool registered=RegisterRawInputDevices(devices,2,
                                                   sizeof(RAWINPUTDEVICE))!=FALSE;
    if(!registered) {
        std::cerr<<"SKIP: Raw Input registration unavailable in this desktop session\n";
        DestroyWindow(hwnd);UnregisterClassW(klass,instance);return 77;
    }
    // Confirm actual Win32 dispatch, not fake injection through WM_INPUT.
    SendMessageW(hwnd,WM_APP+1,0,0);
    if(!probe.synthetic_window_message_seen) {
        std::cerr<<"Window message dispatch failed\n";
        return 1;
    }
    if(interactive) {
        ShowWindow(hwnd,SW_SHOW);
        UpdateWindow(hwnd);
        SetFocus(hwnd);
        SetTimer(hwnd,1,8,nullptr);
        MSG msg{};
        while(GetMessageW(&msg,nullptr,0,0)>0) {
            TranslateMessage(&msg);DispatchMessageW(&msg);
            scarlett::input::Event event{};
            while(probe.router.pop(event)) {
                // Do not record per-device identifiers or key presses to disk.
            }
        }
        KillTimer(hwnd,1);
    }else {
        // Hardware-free CI smoke: system API invocation; no fabricated input.
        probe.poll_xinput();
    }
    for(auto& device:devices) {
        device.dwFlags=RIDEV_REMOVE;
        device.hwndTarget=nullptr;
    }
    const bool removed=RegisterRawInputDevices(devices,2,
                                               sizeof(RAWINPUTDEVICE))!=FALSE;
    if(!interactive)DestroyWindow(hwnd);
    UnregisterClassW(klass,instance);
    if(!removed) {std::cerr<<"Raw Input unregister failed\n";return 1;}
    std::cout<<"{\"kind\":\"native-win32-rawinput-xinput\","
             "\"raw_registration\":true,\"raw_unregistered\":true,"
             "\"native_window_dispatch\":true,"
             "\"xinput_getstate_called\":"<<(probe.xinput_polls?"true":"false")<<','
             <<"\"connected_slots\":"<<probe.connected_slots<<','
             <<"\"raw_messages_received\":"<<probe.raw_messages<<','
             <<"\"rejected_reports\":"<<probe.rejected_reports<<','
             <<"\"clock_frequency_hz\":"<<probe.frequency.QuadPart<<','
             <<"\"actual_device_input_verified\":false,"
             "\"dualsense_haptics_verified\":false,"
             "\"commercial_game_tested\":false}\n";
    return 0;
}
} // namespace
int main(int argc,char** argv) {
    const bool interactive=argc==2 && std::strcmp(argv[1],"--interactive")==0;
    if(argc!=1 && !interactive) {
        std::cerr<<"Usage: native_win_input_probe [--interactive]\n";return 2;
    }
    return run(interactive);
}
