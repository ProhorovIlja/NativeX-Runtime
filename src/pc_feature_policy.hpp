// Original source-only capability/feature decision model for future native ports.
// A "ready" input is *only* a caller assertion, not a detected hardware/SDK fact.
#pragma once
#include <string>
#include <vector>

namespace scarlett {
enum class Os { Windows, Linux };
enum class Vendor { Unknown, Nvidia, Amd, Intel };
enum class Backend { Auto, D3D12, Vulkan, None };
enum class Upscaler { Auto, Native, Fsr, Dlss, Xess };
enum class FrameGen { Auto, Off, Fsr, Dlss, Xess };
enum class Latency { Auto, Engine, Reflex, AntiLag2, Xell };
enum class Storage { Auto, Portable, DirectStorage, IoUring };
enum class Neural { Off, Dlss5 };

struct Preferences {
    Backend backend = Backend::Auto;
    Upscaler upscaler = Upscaler::Auto;
    FrameGen frame_gen = FrameGen::Auto;
    Latency latency = Latency::Auto;
    Storage storage = Storage::Auto;
    Neural neural = Neural::Off;
};
struct Capabilities {
    Os os = Os::Windows;
    Vendor vendor = Vendor::Unknown;
    // These mean verified *app* renderer implementations, not installed DLLs.
    bool d3d12_renderer_ready = false;
    bool vulkan_renderer_ready = false;
    bool temporal_input_ready = false; // verified depth, motion vectors, exposure, history
    bool frame_generation_input_ready = false; // plus UI separation, swapchain hook, pacing
    bool fsr_sr_ready = false;
    bool dlss_sr_ready = false;
    bool xess_sr_ready = false;
    bool fsr_fg_ready = false;
    bool dlss_fg_ready = false;
    bool xess_fg_ready = false;
    bool reflex_ready = false;
    bool anti_lag2_ready = false;
    bool xell_ready = false;
    bool directstorage_ready = false;
    bool io_uring_ready = false;
    // DLSS 5 neural rendering is not an upscaler or frame-generation switch.
    // Public official SDK availability, authorized runtime, renderer and GPU
    // must ALL have been independently verified by the caller.
    bool dlss5_authorized_sdk_ready = false;
    bool dlss5_gpu_driver_approved = false;
    bool neural_scene_guides_ready = false;
};

struct Selection {
    Backend backend = Backend::None;
    Upscaler upscaler = Upscaler::Native;
    FrameGen frame_gen = FrameGen::Off;
    Latency latency = Latency::Engine;
    Storage storage = Storage::Portable;
    Neural neural = Neural::Off;
    bool renderer_usable = false;
    std::vector<std::string> reasons;
};
// Deterministic synthetic policy resolver: no GPU calls, no implied SDK support.
Selection resolve(const Preferences& want, const Capabilities& provided);
const char* name(Backend v);
const char* name(Upscaler v);
const char* name(FrameGen v);
const char* name(Latency v);
const char* name(Storage v);
const char* name(Neural v);
} // namespace scarlett
