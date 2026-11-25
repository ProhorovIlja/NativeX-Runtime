#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// This test runs on a private 24-bit X server so window readback cannot be occluded.
#ifndef VK_EXT_swapchain_maintenance1
int main() {
    std::cout << "SKIP: Vulkan headers lack VK_EXT_swapchain_maintenance1\n";
    return 77;
}
#else
namespace {
struct Unavailable : std::runtime_error {
    using std::runtime_error::runtime_error;
};
void need(bool b, const char *msg) {
    if (!b)
        throw std::runtime_error(msg);
}
void vkcheck(VkResult r, const char *msg) {
    if (r != VK_SUCCESS)
        throw std::runtime_error(std::string(msg) + " returned " + std::to_string(r));
}
template <class T> T info(VkStructureType type) {
    T t{};
    t.sType = type;
    return t;
}
struct Host {
    Display *display = XOpenDisplay(nullptr);
    ::Window window{};
    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice gpu{};
    VkDevice device{};
    uint32_t family{};
    VkQueue queue{};
    VkShaderModule vs{}, fs{};
    VkCommandPool pool{};
    VkCommandBuffer cmd{};
    VkPipelineLayout layout{};
    VkRenderPass pass{};
    VkPipeline pipeline{};
    VkSemaphore acquired{}, drawn{};
    VkFence submitted{}, presented{};
    ~Host() {
        if (device) {
            vkDeviceWaitIdle(device);
            vkDestroyFence(device, presented, nullptr);
            vkDestroyFence(device, submitted, nullptr);
            vkDestroySemaphore(device, drawn, nullptr);
            vkDestroySemaphore(device, acquired, nullptr);
            vkDestroyCommandPool(device, pool, nullptr);
            vkDestroyPipeline(device, pipeline, nullptr);
            vkDestroyPipelineLayout(device, layout, nullptr);
            vkDestroyRenderPass(device, pass, nullptr);
            vkDestroyShaderModule(device, fs, nullptr);
            vkDestroyShaderModule(device, vs, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (surface)
            vkDestroySurfaceKHR(instance, surface, nullptr);
        if (instance)
            vkDestroyInstance(instance, nullptr);
        if (display)
            XCloseDisplay(display);
    }
};
constexpr VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
constexpr uint64_t timeout = 4000000000ull;

VkShaderModule shader(VkDevice dev, const char *name) {
    std::ifstream f(name, std::ios::binary | std::ios::ate);
    const std::streamoff n = f.tellg();
    need(n >= 20 && n <= 1024 * 1024 && n % 4 == 0, "bad shader");
    std::vector<uint32_t> words(n / 4);
    f.seekg(0);
    need(bool(f.read(reinterpret_cast<char *>(words.data()), n)) && words[0] == 0x07230203u, "bad SPIR-V");
    auto s = info<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    s.codeSize = n;
    s.pCode = words.data();
    VkShaderModule out{};
    vkcheck(vkCreateShaderModule(dev, &s, nullptr, &out), "create shader");
    return out;
}

bool select_device(Host &h, VkPhysicalDevice gpu) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(gpu, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_1)
        return false;
    uint32_t n = 0;
    vkcheck(vkEnumerateDeviceExtensionProperties(gpu, nullptr, &n, nullptr), "count device extensions");
    std::vector<VkExtensionProperties> extensions(n);
    vkcheck(vkEnumerateDeviceExtensionProperties(gpu, nullptr, &n, extensions.data()),
            "get device extensions");
    for (const char *name :
         {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME}) {
        if (std::none_of(extensions.begin(), extensions.end(),
                         [&](const auto &e) { return std::strcmp(e.extensionName, name) == 0; }))
            return false;
    }
    auto maintenance = info<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
    auto features = info<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
    features.pNext = &maintenance;
    vkGetPhysicalDeviceFeatures2(gpu, &features);
    if (!maintenance.swapchainMaintenance1)
        return false;
    vkcheck(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, h.surface, &n, nullptr), "count surface formats");
    std::vector<VkSurfaceFormatKHR> formats(n);
    vkcheck(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, h.surface, &n, formats.data()), "get surface formats");
    if (std::none_of(formats.begin(), formats.end(), [](auto f) {
            return f.format == format && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }))
        return false;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n, nullptr);
    std::vector<VkQueueFamilyProperties> families(n);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &n, families.data());
    for (uint32_t i = 0; i < n; ++i) {
        VkBool32 present{};
        vkcheck(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, h.surface, &present), "get surface support");
        if (present && families[i].queueCount && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            h.gpu = gpu;
            h.family = i;
            return true;
        }
    }
    return false;
}

void setup(Host &h, const char *vert, const char *frag) {
    if (!h.display)
        throw Unavailable("X11 display unavailable");
    const int screen = DefaultScreen(h.display);
    const Visual *visual = DefaultVisual(h.display, screen);
    if (DefaultDepth(h.display, screen) != 24 || visual->red_mask != 0xff0000 ||
        visual->green_mask != 0xff00 || visual->blue_mask != 0xff || DisplayWidth(h.display, screen) < 160 ||
        DisplayHeight(h.display, screen) < 121)
        throw Unavailable("requires a 24-bit RGB888 X11 display at least 160x121");
    h.window = XCreateSimpleWindow(h.display, DefaultRootWindow(h.display), 0, 0, 96, 96, 0, 0, 0);
    XMapWindow(h.display, h.window);
    XSync(h.display, False);
    auto app = info<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
    app.apiVersion = VK_API_VERSION_1_1;
    const char *ix[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
                        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
                        VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME};
    auto ic = info<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    ic.pApplicationInfo = &app;
    ic.enabledExtensionCount = 4;
    ic.ppEnabledExtensionNames = ix;
    const VkResult instance_result = vkCreateInstance(&ic, nullptr, &h.instance);
    if (instance_result == VK_ERROR_EXTENSION_NOT_PRESENT || instance_result == VK_ERROR_INCOMPATIBLE_DRIVER)
        throw Unavailable("Vulkan 1.1 Xlib surface or surface maintenance extension unavailable");
    vkcheck(instance_result, "create instance");
    auto xs = info<VkXlibSurfaceCreateInfoKHR>(VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR);
    xs.dpy = h.display;
    xs.window = h.window;
    vkcheck(vkCreateXlibSurfaceKHR(h.instance, &xs, nullptr, &h.surface), "create surface");
    uint32_t n = 0;
    vkcheck(vkEnumeratePhysicalDevices(h.instance, &n, nullptr), "count devices");
    std::vector<VkPhysicalDevice> devices(n);
    if (n)
        vkcheck(vkEnumeratePhysicalDevices(h.instance, &n, devices.data()), "enumerate devices");
    if (std::none_of(devices.begin(), devices.end(), [&](auto gpu) { return select_device(h, gpu); }))
        throw Unavailable("no graphics/present queue with BGRA8 UNORM and swapchain maintenance fences");
    auto maintenance = info<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT>(
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT);
    maintenance.swapchainMaintenance1 = VK_TRUE;
    auto q = info<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
    float priority = 1;
    q.queueCount = 1;
    q.pQueuePriorities = &priority;
    q.queueFamilyIndex = h.family;
    const char *dx[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_EXT_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
    auto dc = info<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    dc.pNext = &maintenance;
    dc.queueCreateInfoCount = 1;
    dc.pQueueCreateInfos = &q;
    dc.enabledExtensionCount = 2;
    dc.ppEnabledExtensionNames = dx;
    vkcheck(vkCreateDevice(h.gpu, &dc, nullptr, &h.device), "create device");
    vkGetDeviceQueue(h.device, h.family, 0, &h.queue);
    h.vs = shader(h.device, vert);
    h.fs = shader(h.device, frag);
    auto pool = info<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = h.family;
    vkcheck(vkCreateCommandPool(h.device, &pool, nullptr, &h.pool), "command pool");
    auto alloc = info<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    alloc.commandPool = h.pool;
    alloc.commandBufferCount = 1;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkcheck(vkAllocateCommandBuffers(h.device, &alloc, &h.cmd), "command buffer");
    auto sc = info<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    vkcheck(vkCreateSemaphore(h.device, &sc, nullptr, &h.acquired), "acquire semaphore");
    vkcheck(vkCreateSemaphore(h.device, &sc, nullptr, &h.drawn), "draw semaphore");
    auto fc = info<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    vkcheck(vkCreateFence(h.device, &fc, nullptr, &h.submitted), "submit fence");
    vkcheck(vkCreateFence(h.device, &fc, nullptr, &h.presented), "present fence");
}

void create_pipeline(Host &h) {
    VkAttachmentDescription a{};
    a.format = format;
    a.samples = VK_SAMPLE_COUNT_1_BIT;
    a.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    a.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference color{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &color;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    auto pass = info<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    pass.attachmentCount = 1;
    pass.pAttachments = &a;
    pass.subpassCount = 1;
    pass.pSubpasses = &sub;
    pass.dependencyCount = 1;
    pass.pDependencies = &dep;
    vkcheck(vkCreateRenderPass(h.device, &pass, nullptr, &h.pass), "render pass");
    auto layout = info<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    vkcheck(vkCreatePipelineLayout(h.device, &layout, nullptr, &h.layout), "pipeline layout");
    auto vs = info<VkPipelineShaderStageCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    vs.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vs.module = h.vs;
    vs.pName = "main";
    auto fs = vs;
    fs.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs.module = h.fs;
    const std::array stages{vs, fs};
    auto vi =
        info<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    auto ia = info<VkPipelineInputAssemblyStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    auto vp = info<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    vp.viewportCount = vp.scissorCount = 1;
    auto rs = info<VkPipelineRasterizationStateCreateInfo>(
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    rs.lineWidth = 1;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    auto ms =
        info<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ca{};
    ca.colorWriteMask = 15;
    auto cb =
        info<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
    cb.attachmentCount = 1;
    cb.pAttachments = &ca;
    VkDynamicState dynamic[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    auto dy = info<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dynamic;
    auto pc = info<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
    pc.stageCount = 2;
    pc.pStages = stages.data();
    pc.pVertexInputState = &vi;
    pc.pInputAssemblyState = &ia;
    pc.pViewportState = &vp;
    pc.pRasterizationState = &rs;
    pc.pMultisampleState = &ms;
    pc.pColorBlendState = &cb;
    pc.pDynamicState = &dy;
    pc.layout = h.layout;
    pc.renderPass = h.pass;
    vkcheck(vkCreateGraphicsPipelines(h.device, VK_NULL_HANDLE, 1, &pc, nullptr, &h.pipeline),
            "graphics pipeline");
}

struct Chain {
    Host &h;
    VkSwapchainKHR id{};
    VkExtent2D extent{};
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> buffers;
    explicit Chain(Host &host) : h(host) {}
    ~Chain() {
        if (std::uncaught_exceptions())
            vkDeviceWaitIdle(h.device);
        for (auto fb : buffers)
            vkDestroyFramebuffer(h.device, fb, nullptr);
        for (auto view : views)
            vkDestroyImageView(h.device, view, nullptr);
        if (id)
            vkDestroySwapchainKHR(h.device, id, nullptr);
    }
    void create(VkExtent2D target, VkSwapchainKHR old) {
        VkSurfaceCapabilitiesKHR caps{};
        vkcheck(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(h.gpu, h.surface, &caps), "get extent");
        extent = caps.currentExtent.width == UINT32_MAX ? target : caps.currentExtent;
        need(extent.width == target.width && extent.height == target.height, "Vulkan/X11 extent mismatch");
        auto s = info<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
        s.surface = h.surface;
        s.minImageCount = caps.minImageCount;
        s.imageFormat = format;
        s.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        s.imageExtent = extent;
        s.imageArrayLayers = 1;
        s.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        s.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        s.preTransform = caps.currentTransform;
        s.compositeAlpha = static_cast<VkCompositeAlphaFlagBitsKHR>(caps.supportedCompositeAlpha &
                                                                    -caps.supportedCompositeAlpha);
        s.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        s.clipped = VK_TRUE;
        s.oldSwapchain = old;
        vkcheck(vkCreateSwapchainKHR(h.device, &s, nullptr, &id), "create swapchain");
        uint32_t n = 0;
        vkcheck(vkGetSwapchainImagesKHR(h.device, id, &n, nullptr), "count images");
        std::vector<VkImage> images(n);
        vkcheck(vkGetSwapchainImagesKHR(h.device, id, &n, images.data()), "get images");
        for (auto image : images) {
            auto view = info<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
            view.image = image;
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = format;
            view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            VkImageView handle{};
            vkcheck(vkCreateImageView(h.device, &view, nullptr, &handle), "image view");
            views.push_back(handle);
            auto fb = info<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
            fb.renderPass = h.pass;
            fb.attachmentCount = 1;
            fb.pAttachments = &handle;
            fb.width = extent.width;
            fb.height = extent.height;
            fb.layers = 1;
            VkFramebuffer buffer{};
            vkcheck(vkCreateFramebuffer(h.device, &fb, nullptr, &buffer), "framebuffer");
            buffers.push_back(buffer);
        }
    }
};

void draw(Host &h, const Chain &c, bool green) {
    uint32_t index = 0;
    vkcheck(vkAcquireNextImageKHR(h.device, c.id, timeout, h.acquired, VK_NULL_HANDLE, &index), "acquire");
    vkcheck(vkResetCommandBuffer(h.cmd, 0), "reset command buffer");
    auto begin = info<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    vkcheck(vkBeginCommandBuffer(h.cmd, &begin), "begin command buffer");
    VkClearValue clear{};
    clear.color.float32[1] = green ? 1.0f : 0.0f;
    clear.color.float32[3] = 1;
    auto rp = info<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    rp.renderPass = h.pass;
    rp.framebuffer = c.buffers.at(index);
    rp.renderArea.extent = c.extent;
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;
    vkCmdBeginRenderPass(h.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(h.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, h.pipeline);
    VkViewport vp{0, 0, float(c.extent.width), float(c.extent.height), 0, 1};
    VkRect2D sc{{0, 0}, c.extent};
    vkCmdSetViewport(h.cmd, 0, 1, &vp);
    vkCmdSetScissor(h.cmd, 0, 1, &sc);
    vkCmdDraw(h.cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(h.cmd);
    vkcheck(vkEndCommandBuffer(h.cmd), "end command buffer");
    VkPipelineStageFlags stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    auto submit = info<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &h.acquired;
    submit.pWaitDstStageMask = &stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &h.cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &h.drawn;
    vkcheck(vkQueueSubmit(h.queue, 1, &submit, h.submitted), "submit");
    auto pf = info<VkSwapchainPresentFenceInfoEXT>(VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT);
    pf.swapchainCount = 1;
    pf.pFences = &h.presented;
    auto present = info<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
    present.pNext = &pf;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &h.drawn;
    present.swapchainCount = 1;
    present.pSwapchains = &c.id;
    present.pImageIndices = &index;
    vkcheck(vkQueuePresentKHR(h.queue, &present), "present");
    // The present fence also covers consumption of the binary render-finished semaphore.
    const VkFence fences[] = {h.submitted, h.presented};
    vkcheck(vkWaitForFences(h.device, 2, fences, VK_TRUE, timeout), "wait for submit and present fences");
    vkcheck(vkResetFences(h.device, 2, fences), "reset fences");
}

struct ImageDeleter {
    void operator()(XImage *p) const {
        if (p)
            XDestroyImage(p);
    }
};
void verify_window(const Host &h, VkExtent2D size, bool green) {
    unsigned int w = size.width, ht = size.height;
    for (int attempt = 0; attempt < 90; ++attempt) {
        std::unique_ptr<XImage, ImageDeleter> img(
            XGetImage(h.display, h.window, 0, 0, w, ht, AllPlanes, ZPixmap));
        need(bool(img), "X11 image readback failed");
        need(img->red_mask == 0xff0000 && img->green_mask == 0xff00 && img->blue_mask == 0xff,
             "expected Xvfb RGB masks");
        bool valid = true;
        unsigned inside = 0, outside = 0;
        for (unsigned y = 0; y < ht && valid; ++y)
            for (unsigned x = 0; x < w; ++x) {
                auto rgb = XGetPixel(img.get(), x, y) & 0xfffffful;
                const int red = (rgb >> 16) & 255, g = (rgb >> 8) & 255, blue = rgb & 255;
                const bool triangle = std::abs(red - 64) <= 1 && std::abs(g - 128) <= 1 && blue == 255;
                const bool background = rgb == (green ? 0x00ff00ul : 0ul);
                double u = (x + 0.5) / w, v = (y + 0.5) / ht;
                double lhs = 0.05 + 0.5 * (v - 0.05);
                double margin = std::min({v - 0.05, 0.95 - v, u - lhs, 1.0 - lhs - u});
                // Near a triangle edge either legal color is accepted to allow rasterization rounding.
                const double band = 1.5 / std::min(w, ht);
                if ((!triangle && !background) || (margin > band && !triangle) ||
                    (margin < -band && !background)) {
                    valid = false;
                    break;
                }
                inside += triangle;
                outside += background;
            }
        if (valid && inside > 0 && outside > 0)
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    throw std::runtime_error("window pixels differ from the triangle and background oracle");
}

} // namespace

int main(int argc, char **argv) try {
    if (argc != 3)
        return 2;
    Host h;
    setup(h, argv[1], argv[2]);
    create_pipeline(h);
    std::array<VkExtent2D, 4> sizes{{{96, 96}, {160, 90}, {73, 121}, {96, 96}}};
    std::unique_ptr<Chain> chain;
    uint64_t samples = 0;
    for (auto size : sizes) {
        XResizeWindow(h.display, h.window, size.width, size.height);
        XSync(h.display, False);
        XWindowAttributes attr{};
        need(XGetWindowAttributes(h.display, h.window, &attr) && attr.width == int(size.width) &&
                 attr.height == int(size.height),
             "X11 resize failed");
        auto next = std::make_unique<Chain>(h);
        next->create(size, chain ? chain->id : VK_NULL_HANDLE);
        // All work and presents on the old chain completed before the resize.
        chain = std::move(next);
        for (bool green : {false, true}) {
            draw(h, *chain, green);
            verify_window(h, size, green);
            samples += uint64_t(size.width) * size.height;
        }
        std::cout << size.width << "x" << size.height << " frames=2 full_window_checks=2\n";
    }
    std::cout << "swapchains=4 replacements=3 presents=8 full_window_pixels=" << samples << "\n";
    return 0;
} catch (const Unavailable &e) {
    std::cout << "SKIP: " << e.what() << '\n';
    return 77;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}

#endif
