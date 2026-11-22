// Original Linux x86-64 ELF Vulkan graphics pipeline and offscreen pixel verification.
// Shader source is built locally with glslangValidator, not a prebuilt binary.
// No window, swapchain, game assets, Xbox command packets or real GPU claimed.
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
constexpr std::uint32_t Width=32, Height=32;
constexpr VkDeviceSize ImageBytes=Width*Height*4u;
void check(VkResult code,const char* label){
    if(code!=VK_SUCCESS)
        throw std::runtime_error(std::string(label)+" VkResult="+std::to_string(code));
}
struct Objects {
    VkInstance instance=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkImage image=VK_NULL_HANDLE;
    VkDeviceMemory image_memory=VK_NULL_HANDLE;
    VkImageView image_view=VK_NULL_HANDLE;
    VkBuffer readback=VK_NULL_HANDLE;
    VkDeviceMemory readback_memory=VK_NULL_HANDLE;
    VkRenderPass pass=VK_NULL_HANDLE;
    VkFramebuffer framebuffer=VK_NULL_HANDLE;
    VkPipelineLayout layout=VK_NULL_HANDLE;
    VkShaderModule vs=VK_NULL_HANDLE, ps=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkCommandPool pool=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    void* mapped=nullptr;
    ~Objects(){
        if(device!=VK_NULL_HANDLE){
            vkDeviceWaitIdle(device);
            if(mapped)vkUnmapMemory(device,readback_memory);
            if(fence)vkDestroyFence(device,fence,nullptr);
            if(pool)vkDestroyCommandPool(device,pool,nullptr);
            if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
            if(vs)vkDestroyShaderModule(device,vs,nullptr);
            if(ps)vkDestroyShaderModule(device,ps,nullptr);
            if(layout)vkDestroyPipelineLayout(device,layout,nullptr);
            if(framebuffer)vkDestroyFramebuffer(device,framebuffer,nullptr);
            if(pass)vkDestroyRenderPass(device,pass,nullptr);
            if(image_view)vkDestroyImageView(device,image_view,nullptr);
            if(readback)vkDestroyBuffer(device,readback,nullptr);
            if(image)vkDestroyImage(device,image,nullptr);
            if(readback_memory)vkFreeMemory(device,readback_memory,nullptr);
            if(image_memory)vkFreeMemory(device,image_memory,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    }
};
std::vector<std::uint32_t> load_spirv(const char* path){
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)throw std::runtime_error("SPIR-V not found");
    const auto n=static_cast<std::streamoff>(file.tellg());
    if(n<20 || n>1024*1024 || n%4!=0)
        throw std::runtime_error("SPIR-V unexpected length");
    std::vector<std::uint32_t> words(static_cast<std::size_t>(n/4));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(words.data()),n);
    if(!file || words[0]!=0x07230203u)
        throw std::runtime_error("SPIR-V header corrupted");
    return words;
}
std::uint32_t choose_memory(const VkPhysicalDeviceMemoryProperties& memory,
                            std::uint32_t bits,VkMemoryPropertyFlags flags){
    for(std::uint32_t i=0;i<memory.memoryTypeCount;i++){
        if((bits&(1u<<i)) &&
          (memory.memoryTypes[i].propertyFlags&flags)==flags)return i;
    }
    return std::numeric_limits<std::uint32_t>::max();
}
VkShaderModule shader(VkDevice device,const std::vector<std::uint32_t>& code){
    VkShaderModuleCreateInfo info{};
    info.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize=code.size()*4u;
    info.pCode=code.data();
    VkShaderModule result=VK_NULL_HANDLE;
    check(vkCreateShaderModule(device,&info,nullptr,&result),"create SPIR-V shader module");
    return result;
}
bool approx(std::uint8_t value,std::uint32_t target){
    const auto x=static_cast<std::uint32_t>(value);
    return x+2u>=target && x<=target+2u;
}
bool rgba(const std::uint8_t* ptr,std::uint32_t x,std::uint32_t y,
          std::uint32_t r,std::uint32_t g,std::uint32_t b,std::uint32_t a){
    const auto* color=ptr+(y*Width+x)*4;
    return approx(color[0],r)&&approx(color[1],g)&&
           approx(color[2],b)&&approx(color[3],a);
}
int run(const char* vert_path,const char* frag_path){
    const auto vert=load_spirv(vert_path), frag=load_spirv(frag_path);
    Objects object{};
    VkApplicationInfo app{};
    app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName="Scarlett Original Native Vulkan Raster";
    app.apiVersion=VK_API_VERSION_1_0;
    VkInstanceCreateInfo instance_ci{};
    instance_ci.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo=&app;
    check(vkCreateInstance(&instance_ci,nullptr,&object.instance),"create Vulkan instance");
    std::uint32_t num=0;
    check(vkEnumeratePhysicalDevices(object.instance,&num,nullptr),"enumerate Vulkan devices");
    if(!num){std::cout<<"SKIP: no Vulkan device\n";return 77;}
    std::vector<VkPhysicalDevice> candidates(num);
    check(vkEnumeratePhysicalDevices(object.instance,&num,candidates.data()),
          "list Vulkan physical devices");
    VkPhysicalDevice physical=VK_NULL_HANDLE;
    std::uint32_t family=0;
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceMemoryProperties memory{};
    for(auto current:candidates){
        VkFormatProperties format{};
        vkGetPhysicalDeviceFormatProperties(current,VK_FORMAT_R8G8B8A8_UNORM,&format);
        if(!(format.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT))
            continue;
        std::uint32_t count=0;
        vkGetPhysicalDeviceQueueFamilyProperties(current,&count,nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(current,&count,families.data());
        for(std::uint32_t i=0;i<count;i++){
            if(families[i].queueCount && (families[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){
                physical=current;
                family=i;
                vkGetPhysicalDeviceProperties(current,&props);
                vkGetPhysicalDeviceMemoryProperties(current,&memory);
                break;
            }
        }
        if(physical)break;
    }
    if(!physical){std::cout<<"SKIP: no graphics queue with RGBA8 render target\n";return 77;}
    float priority=1.0f;
    VkDeviceQueueCreateInfo queue_ci{};
    queue_ci.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_ci.queueFamilyIndex=family;
    queue_ci.queueCount=1;
    queue_ci.pQueuePriorities=&priority;
    VkDeviceCreateInfo device_ci{};
    device_ci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount=1;
    device_ci.pQueueCreateInfos=&queue_ci;
    check(vkCreateDevice(physical,&device_ci,nullptr,&object.device),"create Vulkan device");
    VkQueue queue=VK_NULL_HANDLE;
    vkGetDeviceQueue(object.device,family,0,&queue);
    if(!queue)throw std::runtime_error("graphics queue missing");

    VkImageCreateInfo image_ci{};
    image_ci.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_ci.imageType=VK_IMAGE_TYPE_2D;
    image_ci.format=VK_FORMAT_R8G8B8A8_UNORM;
    image_ci.extent={Width,Height,1};
    image_ci.mipLevels=1;
    image_ci.arrayLayers=1;
    image_ci.samples=VK_SAMPLE_COUNT_1_BIT;
    image_ci.tiling=VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    image_ci.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    check(vkCreateImage(object.device,&image_ci,nullptr,&object.image),"create color image");
    VkMemoryRequirements image_req{};
    vkGetImageMemoryRequirements(object.device,object.image,&image_req);
    auto image_type=choose_memory(memory,image_req.memoryTypeBits,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if(image_type==std::numeric_limits<std::uint32_t>::max())
        image_type=choose_memory(memory,image_req.memoryTypeBits,0);
    if(image_type==std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("no compatible image memory type");
    VkMemoryAllocateInfo image_alloc{};
    image_alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    image_alloc.allocationSize=image_req.size;
    image_alloc.memoryTypeIndex=image_type;
    check(vkAllocateMemory(object.device,&image_alloc,nullptr,&object.image_memory),
          "allocate image memory");
    check(vkBindImageMemory(object.device,object.image,object.image_memory,0),
          "bind image memory");
    VkImageViewCreateInfo view_ci{};
    view_ci.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_ci.image=object.image;
    view_ci.viewType=VK_IMAGE_VIEW_TYPE_2D;
    view_ci.format=VK_FORMAT_R8G8B8A8_UNORM;
    view_ci.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    view_ci.subresourceRange.levelCount=1;
    view_ci.subresourceRange.layerCount=1;
    check(vkCreateImageView(object.device,&view_ci,nullptr,&object.image_view),
          "create image view");

    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size=ImageBytes;
    buffer_ci.usage=VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buffer_ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(object.device,&buffer_ci,nullptr,&object.readback),
          "create CPU readback buffer");
    VkMemoryRequirements buffer_req{};
    vkGetBufferMemoryRequirements(object.device,object.readback,&buffer_req);
    const auto host_type=choose_memory(memory,buffer_req.memoryTypeBits,
              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if(host_type==std::numeric_limits<std::uint32_t>::max()){
        std::cout<<"SKIP: no host-visible coherent readback memory\n";return 77;
    }
    VkMemoryAllocateInfo buffer_alloc{};
    buffer_alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    buffer_alloc.allocationSize=buffer_req.size;
    buffer_alloc.memoryTypeIndex=host_type;
    check(vkAllocateMemory(object.device,&buffer_alloc,nullptr,&object.readback_memory),
          "allocate readback memory");
    check(vkBindBufferMemory(object.device,object.readback,object.readback_memory,0),
          "bind readback memory");

    VkAttachmentDescription color{};
    color.format=VK_FORMAT_R8G8B8A8_UNORM;
    color.samples=VK_SAMPLE_COUNT_1_BIT;
    color.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp=VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
    color.finalLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference attachment{};
    attachment.attachment=0;
    attachment.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount=1;
    subpass.pColorAttachments=&attachment;
    VkSubpassDependency dependency{};
    dependency.srcSubpass=0;
    dependency.dstSubpass=VK_SUBPASS_EXTERNAL;
    dependency.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;
    dependency.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo pass_ci{};
    pass_ci.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pass_ci.attachmentCount=1;
    pass_ci.pAttachments=&color;
    pass_ci.subpassCount=1;
    pass_ci.pSubpasses=&subpass;
    pass_ci.dependencyCount=1;
    pass_ci.pDependencies=&dependency;
    check(vkCreateRenderPass(object.device,&pass_ci,nullptr,&object.pass),
          "create Vulkan render pass");
    VkFramebufferCreateInfo framebuffer_ci{};
    framebuffer_ci.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_ci.renderPass=object.pass;
    framebuffer_ci.attachmentCount=1;
    framebuffer_ci.pAttachments=&object.image_view;
    framebuffer_ci.width=Width;
    framebuffer_ci.height=Height;
    framebuffer_ci.layers=1;
    check(vkCreateFramebuffer(object.device,&framebuffer_ci,nullptr,&object.framebuffer),
          "create framebuffer");

    object.vs=shader(object.device,vert);
    object.ps=shader(object.device,frag);
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage=VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module=object.vs;
    stages[0].pName="main";
    stages[1].sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage=VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module=object.ps;
    stages[1].pName="main";
    VkPipelineVertexInputStateCreateInfo vertex_ci{};
    vertex_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{0.0f,0.0f,static_cast<float>(Width),static_cast<float>(Height),
                        0.0f,1.0f};
    VkRect2D scissor{};
    scissor.extent={Width,Height};
    VkPipelineViewportStateCreateInfo viewport_ci{};
    viewport_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_ci.viewportCount=1;
    viewport_ci.pViewports=&viewport;
    viewport_ci.scissorCount=1;
    viewport_ci.pScissors=&scissor;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode=VK_POLYGON_MODE_FILL;
    raster.cullMode=VK_CULL_MODE_NONE;
    raster.frontFace=VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth=1.0f;
    VkPipelineMultisampleStateCreateInfo multi{};
    multi.sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multi.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blend_attachment{};
    blend_attachment.colorWriteMask=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|
                                    VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount=1;
    blend.pAttachments=&blend_attachment;
    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    check(vkCreatePipelineLayout(object.device,&layout_ci,nullptr,&object.layout),
          "create graphics pipeline layout");
    VkGraphicsPipelineCreateInfo pipe_ci{};
    pipe_ci.sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipe_ci.stageCount=2;
    pipe_ci.pStages=stages;
    pipe_ci.pVertexInputState=&vertex_ci;
    pipe_ci.pInputAssemblyState=&assembly;
    pipe_ci.pViewportState=&viewport_ci;
    pipe_ci.pRasterizationState=&raster;
    pipe_ci.pMultisampleState=&multi;
    pipe_ci.pColorBlendState=&blend;
    pipe_ci.layout=object.layout;
    pipe_ci.renderPass=object.pass;
    pipe_ci.subpass=0;
    check(vkCreateGraphicsPipelines(object.device,VK_NULL_HANDLE,1,&pipe_ci,
                                   nullptr,&object.pipeline),"create graphics pipeline");

    VkCommandPoolCreateInfo pool_ci{};
    pool_ci.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_ci.queueFamilyIndex=family;
    check(vkCreateCommandPool(object.device,&pool_ci,nullptr,&object.pool),
          "create graphics command pool");
    VkCommandBufferAllocateInfo cmd_alloc{};
    cmd_alloc.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmd_alloc.commandPool=object.pool;
    cmd_alloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd_alloc.commandBufferCount=1;
    VkCommandBuffer command=VK_NULL_HANDLE;
    check(vkAllocateCommandBuffers(object.device,&cmd_alloc,&command),
          "allocate graphics command buffer");
    VkCommandBufferBeginInfo begin{};
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(command,&begin),"begin raster command buffer");
    VkClearValue clear{};
    clear.color.float32[0]=0.0f;
    clear.color.float32[1]=0.0f;
    clear.color.float32[2]=0.0f;
    clear.color.float32[3]=1.0f;
    VkRenderPassBeginInfo pass_begin{};
    pass_begin.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass_begin.renderPass=object.pass;
    pass_begin.framebuffer=object.framebuffer;
    pass_begin.renderArea.extent={Width,Height};
    pass_begin.clearValueCount=1;
    pass_begin.pClearValues=&clear;
    vkCmdBeginRenderPass(command,&pass_begin,VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_GRAPHICS,object.pipeline);
    vkCmdDraw(command,3,1,0,0);
    vkCmdEndRenderPass(command);

    VkBufferImageCopy copy{};
    copy.bufferOffset=0;
    copy.imageSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount=1;
    copy.imageExtent={Width,Height,1};
    vkCmdCopyImageToBuffer(command,object.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           object.readback,1,&copy);
    VkBufferMemoryBarrier barrier{};
    barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer=object.readback;
    barrier.size=ImageBytes;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    check(vkEndCommandBuffer(command),"end raster command buffer");
    VkFenceCreateInfo fence_ci{};
    fence_ci.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    check(vkCreateFence(object.device,&fence_ci,nullptr,&object.fence),
          "create graphics submit fence");
    VkSubmitInfo submit{};
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount=1;
    submit.pCommandBuffers=&command;
    check(vkQueueSubmit(queue,1,&submit,object.fence),"submit raster command buffer");
    check(vkWaitForFences(object.device,1,&object.fence,VK_TRUE,10000000000ull),
          "wait for raster fence");
    check(vkMapMemory(object.device,object.readback_memory,0,ImageBytes,0,&object.mapped),
          "map RGBA8 CPU readback");
    const auto* data=static_cast<const std::uint8_t*>(object.mapped);
    const bool center=rgba(data,16,16,64,128,255,255);
    const bool outside=rgba(data,0,0,0,0,0,255) &&
                       rgba(data,31,31,0,0,0,255);
    std::uint32_t drawn=0,background=0;
    for(std::uint32_t i=0;i<Width*Height;i++){
        if(approx(data[i*4],64)&&approx(data[i*4+1],128)&&
           approx(data[i*4+2],255)&&approx(data[i*4+3],255))++drawn;
        else if(data[i*4]==0 && data[i*4+1]==0 &&
                data[i*4+2]==0 && data[i*4+3]==255)++background;
        else throw std::runtime_error("Unexpected color at pixel "+std::to_string(i));
    }
    if(!center || !outside || drawn<200 || background<200)
        throw std::runtime_error("Raster triangle/background CPU pixel oracle mismatch");
    std::cout<<"{\"kind\":\"native-linux-elf-vulkan-raster\","
             "\"device_type\":"<<static_cast<std::uint32_t>(props.deviceType)<<","
             "\"vendor_id\":"<<props.vendorID<<","
             "\"dimensions\":\"32x32\","
             "\"raster_pixels\":"<<drawn<<","
             "\"background_pixels\":"<<background<<","
             "\"pixel_readback_verified\":true,"
             "\"graphics_draw_submitted\":true,"
             "\"queue_fence_completed\":true,"
             "\"window_or_present_tested\":false,"
             "\"physical_gpu_verified\":false,"
             "\"xbox_shader_tested\":false,"
             "\"gta_vi_tested\":false}\n";
    return 0;
}
}
int main(int argc,char** argv){
    if(argc!=3){
        std::cerr<<"Usage: linux_vulkan_raster_readback shader.vert.spv shader.frag.spv\n";
        return 2;
    }
    try{return run(argv[1],argv[2]);}
    catch(const std::exception& ex){
        std::cerr<<"native Linux Vulkan raster FAILED: "<<ex.what()<<"\n";
        return 1;
    }
}
