#include "pc_feature_policy.hpp"

namespace scarlett {
namespace {
bool backend_ready(const Capabilities& c,Backend b) {
    if (b==Backend::D3D12) return c.os==Os::Windows && c.d3d12_renderer_ready;
    if (b==Backend::Vulkan) return c.vulkan_renderer_ready;
    return false;
}
bool sr_supported(const Capabilities& c,Upscaler u) {
    if (!c.temporal_input_ready) return false;
    switch(u) {
        case Upscaler::Fsr: return c.fsr_sr_ready;
        case Upscaler::Dlss: return c.vendor==Vendor::Nvidia && c.dlss_sr_ready;
        case Upscaler::Xess: return c.xess_sr_ready;
        default: return false;
    }
}
Upscaler best_sr(const Capabilities& c) {
    if (c.vendor==Vendor::Nvidia && sr_supported(c,Upscaler::Dlss)) return Upscaler::Dlss;
    if (c.vendor==Vendor::Intel && sr_supported(c,Upscaler::Xess)) return Upscaler::Xess;
    if (sr_supported(c,Upscaler::Fsr)) return Upscaler::Fsr;
    if (sr_supported(c,Upscaler::Xess)) return Upscaler::Xess;
    return Upscaler::Native;
}
bool fg_supported(const Capabilities& c,FrameGen f) {
    if (!c.frame_generation_input_ready || !c.temporal_input_ready) return false;
    switch(f) {
        case FrameGen::Fsr: return c.fsr_fg_ready;
        case FrameGen::Dlss: return c.vendor==Vendor::Nvidia && c.dlss_fg_ready;
        case FrameGen::Xess: return c.xess_fg_ready;
        default: return false;
    }
}
FrameGen best_fg(const Capabilities& c) {
    if (c.vendor==Vendor::Nvidia && fg_supported(c,FrameGen::Dlss)) return FrameGen::Dlss;
    if (c.vendor==Vendor::Intel && fg_supported(c,FrameGen::Xess)) return FrameGen::Xess;
    if (fg_supported(c,FrameGen::Fsr)) return FrameGen::Fsr;
    if (fg_supported(c,FrameGen::Xess)) return FrameGen::Xess;
    return FrameGen::Off;
}
bool latency_supported(const Capabilities& c,Latency l) {
    switch(l) {
        case Latency::Reflex: return c.vendor==Vendor::Nvidia && c.reflex_ready;
        case Latency::AntiLag2: return c.os==Os::Windows && c.vendor==Vendor::Amd && c.anti_lag2_ready;
        case Latency::Xell: return c.os==Os::Windows && c.xell_ready &&
            (c.vendor==Vendor::Intel || (c.xess_fg_ready && c.frame_generation_input_ready));
        case Latency::Engine: return true;
        default: return false;
    }
}
Latency best_latency(const Capabilities& c) {
    if (latency_supported(c,Latency::Reflex)) return Latency::Reflex;
    if (latency_supported(c,Latency::AntiLag2)) return Latency::AntiLag2;
    if (latency_supported(c,Latency::Xell)) return Latency::Xell;
    return Latency::Engine;
}
bool storage_supported(const Capabilities& c,Storage s) {
    if (s==Storage::DirectStorage) return c.os==Os::Windows && c.directstorage_ready;
    if (s==Storage::IoUring) return c.os==Os::Linux && c.io_uring_ready;
    return s==Storage::Portable;
}
Storage best_storage(const Capabilities& c) {
    if(storage_supported(c,Storage::DirectStorage)) return Storage::DirectStorage;
    if(storage_supported(c,Storage::IoUring)) return Storage::IoUring;
    return Storage::Portable;
}
} // namespace

Selection resolve(const Preferences& want,const Capabilities& c) {
    Selection out;
    if(want.backend==Backend::Auto) {
        const Backend preferred=c.os==Os::Windows?Backend::D3D12:Backend::Vulkan;
        if(backend_ready(c,preferred)) out.backend=preferred;
        else if(backend_ready(c,Backend::Vulkan)) out.backend=Backend::Vulkan;
    } else if(backend_ready(c,want.backend)) {
        out.backend=want.backend;
    } else {
        out.reasons.emplace_back("requested renderer unavailable or unsupported on selected OS");
    }
    out.renderer_usable=out.backend!=Backend::None;
    if (!out.renderer_usable) {
        out.reasons.emplace_back("no verified native renderer; graphics effects disabled");
    } else {
        if(want.upscaler==Upscaler::Auto) out.upscaler=best_sr(c);
        else if(want.upscaler==Upscaler::Native) out.upscaler=Upscaler::Native;
        else if(sr_supported(c,want.upscaler)) out.upscaler=want.upscaler;
        else {
            out.upscaler=best_sr(c);
            out.reasons.emplace_back("requested upscaler not verified; safe native or supported fallback");
        }
        if(want.frame_gen==FrameGen::Auto) out.frame_gen=best_fg(c);
        else if(want.frame_gen==FrameGen::Off) out.frame_gen=FrameGen::Off;
        else if(fg_supported(c,want.frame_gen)) out.frame_gen=want.frame_gen;
        else {
            out.frame_gen=FrameGen::Off; // no silent substitution between FG providers
            out.reasons.emplace_back("frame generation disabled: unverified provider or frame prerequisites");
        }
        if(want.latency==Latency::Auto) out.latency=best_latency(c);
        else if(latency_supported(c,want.latency)) out.latency=want.latency;
        else {
            out.latency=Latency::Engine;
            out.reasons.emplace_back("requested low-latency API unavailable; engine frame pacing only");
        }
        if(want.neural==Neural::Dlss5) {
            if(c.os==Os::Windows && out.backend==Backend::D3D12 &&
               c.vendor==Vendor::Nvidia && c.dlss5_authorized_sdk_ready &&
               c.dlss5_gpu_driver_approved && c.neural_scene_guides_ready &&
               c.temporal_input_ready) {
                out.neural=Neural::Dlss5;
            } else {
                out.reasons.emplace_back("DLSS5 OFF: authorized SDK, supported NVIDIA GPU/driver, D3D12 and scene guides required");
            }
        }
    }
    if(want.storage==Storage::Auto) out.storage=best_storage(c);
    else if(storage_supported(c,want.storage)) out.storage=want.storage;
    else {
        out.storage=Storage::Portable;
        out.reasons.emplace_back("requested storage backend unavailable; portable async I/O selected");
    }
    return out;
}
const char* name(Backend v) {
    switch(v) {case Backend::Auto:return "auto";case Backend::D3D12:return "d3d12";case Backend::Vulkan:return "vulkan";default:return "none";}
}
const char* name(Upscaler v) {
    switch(v) {case Upscaler::Auto:return "auto";case Upscaler::Native:return "native";case Upscaler::Fsr:return "fsr";case Upscaler::Dlss:return "dlss";default:return "xess";}
}
const char* name(FrameGen v) {
    switch(v) {case FrameGen::Auto:return "auto";case FrameGen::Off:return "off";case FrameGen::Fsr:return "fsr";case FrameGen::Dlss:return "dlss";default:return "xess";}
}
const char* name(Latency v) {
    switch(v) {case Latency::Auto:return "auto";case Latency::Engine:return "engine";case Latency::Reflex:return "reflex";case Latency::AntiLag2:return "anti-lag-2";default:return "xell";}
}
const char* name(Storage v) {
    switch(v) {case Storage::Auto:return "auto";case Storage::Portable:return "portable-async";case Storage::DirectStorage:return "directstorage";default:return "io_uring";}
}
const char* name(Neural v) { return v==Neural::Dlss5?"dlss5-neural":"off"; }
} // namespace scarlett
