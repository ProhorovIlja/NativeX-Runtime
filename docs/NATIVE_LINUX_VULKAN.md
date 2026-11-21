# Native Linux x86-64 ELF Vulkan capability gate

This is an ORIGINAL source-built Linux ELF application, not a Windows PE loaded through Wine/Proton or a CPU emulator. It compiles against the Ubuntu Vulkan SDK, creates a VkInstance, enumerates Vulkan devices, locates a graphics-capable queue family, creates a logical VkDevice, retrieves its queue, waits for idle and cleans up. No third-party game executable or console SDK is involved.

Vulkan physical device enumeration can report software CPU-based Mesa drivers such as llvmpipe/lavapipe. The JSON result reports device type/vendor ID and explicitly says hardware_gpu_verified=false, even if VkDevice creation and graphics-queue retrieval succeed. When no Vulkan-capable device exists, CTest SKIPS with exit 77 instead of falsely claiming a GPU pass. Vulkan API errors fail the CI job.

A dedicated workflow uses only standard free public GitHub Ubuntu runners and distro packages libvulkan-dev, libvulkan1, mesa-vulkan-drivers. It records installed package versions and compiles a native x86-64 ELF program with strict warnings. It does NOT render pixels, Present a window, implement FSR/DLSS/XeSS, translate Xbox RDNA2 ISA or prove a Linux GTA VI game port.

Next Linux milestones: create a real native Wayland/X11 Vulkan WSI swapchain and visible frame with readback comparison, native input/audio/network/storage, then a genuine ELF-targeted game build/translation when lawful complete files exist. Keep direct native Linux separate from Windows PE compatibility experiments.
