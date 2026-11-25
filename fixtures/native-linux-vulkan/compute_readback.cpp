// Original native Linux ELF Vulkan compute dispatch and verified host readback.
// Executes SPIR-V on the selected Vulkan device (Mesa software GPU in CI is expected).
// No proprietary game shaders, assets, Xbox D3D12.X or surface/swapchain.
#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::size_t kElements=256;
constexpr std::size_t kLanesPerGroup = 64;
constexpr std::size_t kInputWordsPerLane = 4;
constexpr std::size_t kPartitionWordsPerLane = 12;
std::uint32_t expected(std::uint32_t i){return (i*37u+11u)^0x5a5aa5a5u;}
void check(VkResult r,const char* label) {
    if(r!=VK_SUCCESS)
        throw std::runtime_error(std::string(label)+" VkResult="+std::to_string(r));
}
struct VkObjects {
    VkInstance instance=VK_NULL_HANDLE;
    VkDevice device=VK_NULL_HANDLE;
    VkBuffer buffer=VK_NULL_HANDLE;
    VkDeviceMemory memory=VK_NULL_HANDLE;
    VkDescriptorSetLayout set_layout=VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool=VK_NULL_HANDLE;
    VkPipelineLayout pipeline_layout=VK_NULL_HANDLE;
    VkShaderModule shader_module=VK_NULL_HANDLE;
    VkPipeline pipeline=VK_NULL_HANDLE;
    VkCommandPool command_pool=VK_NULL_HANDLE;
    VkFence fence=VK_NULL_HANDLE;
    void* mapped=nullptr;
    ~VkObjects(){
        if(device!=VK_NULL_HANDLE){
            vkDeviceWaitIdle(device);
            if(mapped)vkUnmapMemory(device,memory);
            if(fence)vkDestroyFence(device,fence,nullptr);
            if(command_pool)vkDestroyCommandPool(device,command_pool,nullptr);
            if(pipeline)vkDestroyPipeline(device,pipeline,nullptr);
            if(shader_module)vkDestroyShaderModule(device,shader_module,nullptr);
            if(pipeline_layout)vkDestroyPipelineLayout(device,pipeline_layout,nullptr);
            if(descriptor_pool)vkDestroyDescriptorPool(device,descriptor_pool,nullptr);
            if(set_layout)vkDestroyDescriptorSetLayout(device,set_layout,nullptr);
            if(buffer)vkDestroyBuffer(device,buffer,nullptr);
            if(memory)vkFreeMemory(device,memory,nullptr);
            vkDestroyDevice(device,nullptr);
        }
        if(instance)vkDestroyInstance(instance,nullptr);
    }
};
std::vector<std::uint32_t> load_spirv(const char* path){
    std::ifstream in(path,std::ios::binary|std::ios::ate);
    if(!in)throw std::runtime_error("SPIR-V shader missing");
    const std::streamoff end=static_cast<std::streamoff>(in.tellg());
    if(end<=0||end>static_cast<std::streamoff>(1024*1024)||
       end%4!=0)
       throw std::runtime_error("Invalid SPIR-V file length");
    const auto bytes=static_cast<std::size_t>(end);
    std::vector<std::uint32_t> data(bytes/4);
    in.seekg(0);
    in.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(bytes));
    if(!in||data.size()<5||data[0]!=0x07230203u)
        throw std::runtime_error("Invalid SPIR-V magic/header");
    return data;
}
std::vector<std::uint32_t> load_vectors(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const std::streamoff bytes = file.tellg();
    if (bytes <= 0 || bytes > 32 * 1024 * 1024 || bytes % sizeof(std::uint32_t) != 0)
        throw std::runtime_error(std::string("invalid uint32 vector file: ") + path);
    std::vector<std::uint32_t> words(static_cast<std::size_t>(bytes) / sizeof(std::uint32_t));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(words.data()), bytes))
        throw std::runtime_error(std::string("short read: ") + path);
    return words;
}

int test(const char* path, const std::vector<std::uint32_t>& input,
         const std::vector<std::uint32_t>& reference) {
    const bool partition = !input.empty();
    const std::size_t lanes = partition ? input.size() / kInputWordsPerLane : reference.size();
    if (partition && (input.size() % (kLanesPerGroup * kInputWordsPerLane) != 0 ||
                      reference.size() != lanes * kPartitionWordsPerLane))
        throw std::runtime_error("expected 64 lanes per group, 4 input and 12 reference words per lane");
    const auto groups = static_cast<std::uint32_t>(lanes / kLanesPerGroup);
    const VkDeviceSize inputBytes = input.size() * sizeof(std::uint32_t);
    const VkDeviceSize outputBytes = reference.size() * sizeof(std::uint32_t);
    const VkDeviceSize bufferBytes = inputBytes + outputBytes;
    const auto spirv=load_spirv(path);
    VkObjects object{};
    VkApplicationInfo app{};
    app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName="Scarlett Original Native Linux Vulkan Compute";
    app.apiVersion = partition ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
    VkInstanceCreateInfo instance_ci{};
    instance_ci.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_ci.pApplicationInfo=&app;
    const auto instanceResult = vkCreateInstance(&instance_ci, nullptr, &object.instance);
    if (partition && instanceResult == VK_ERROR_INCOMPATIBLE_DRIVER) {
        std::cout << "SKIP: Vulkan 1.1 unavailable\n";
        return 77;
    }
    check(instanceResult,"create instance");
    std::uint32_t count=0;
    check(vkEnumeratePhysicalDevices(object.instance,&count,nullptr),"enumerate devices");
    if(count==0){
        std::cout<<"SKIP: No Vulkan physical devices (software or hardware) available\n";
        return 77;
    }
    std::vector<VkPhysicalDevice> devices(count);
    check(vkEnumeratePhysicalDevices(object.instance,&count,devices.data()),"get devices");
    VkPhysicalDevice selected=VK_NULL_HANDLE;
    std::uint32_t family=0,memory_index=std::numeric_limits<std::uint32_t>::max();
    VkPhysicalDeviceProperties props{};
    // Select a compute queue, prefer a host-visible coherent memory type.
    for(auto device:devices){
        vkGetPhysicalDeviceProperties(device, &props);
        if (partition && (props.apiVersion < VK_API_VERSION_1_1 ||
                          inputBytes % props.limits.minStorageBufferOffsetAlignment != 0 ||
                          outputBytes > props.limits.maxStorageBufferRange ||
                          groups > props.limits.maxComputeWorkGroupCount[0]))
            continue;
        VkPhysicalDeviceMemoryProperties memory_props{};
        vkGetPhysicalDeviceMemoryProperties(device,&memory_props);
        bool supported_memory=false;
        for(std::uint32_t i=0;i<memory_props.memoryTypeCount;i++)
          if((memory_props.memoryTypes[i].propertyFlags&
              (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
              (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
              supported_memory=true;
        if(!supported_memory)continue;
        std::uint32_t n=0;
        vkGetPhysicalDeviceQueueFamilyProperties(device,&n,nullptr);
        std::vector<VkQueueFamilyProperties> families(n);
        vkGetPhysicalDeviceQueueFamilyProperties(device,&n,families.data());
        for(std::uint32_t i=0;i<n;i++){
            if(families[i].queueCount&&(families[i].queueFlags&VK_QUEUE_COMPUTE_BIT)){
                selected=device;family=i;
                break;
            }
        }
        if(selected)break;
    }
    if(!selected){
        std::cout<<"SKIP: No device meets the compute fixture requirements\n";
        return 77;
    }
    const float priority=1.0f;
    VkDeviceQueueCreateInfo queue_ci{};
    queue_ci.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_ci.queueFamilyIndex=family;
    queue_ci.queueCount=1;
    queue_ci.pQueuePriorities=&priority;
    VkDeviceCreateInfo device_ci{};
    device_ci.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_ci.queueCreateInfoCount=1;
    device_ci.pQueueCreateInfos=&queue_ci;
    check(vkCreateDevice(selected,&device_ci,nullptr,&object.device),"create device");
    VkQueue queue=VK_NULL_HANDLE;
    vkGetDeviceQueue(object.device,family,0,&queue);
    if(!queue)throw std::runtime_error("compute queue missing");
    VkBufferCreateInfo buffer_ci{};
    buffer_ci.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.size=bufferBytes;
    buffer_ci.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buffer_ci.sharingMode=VK_SHARING_MODE_EXCLUSIVE;
    check(vkCreateBuffer(object.device,&buffer_ci,nullptr,&object.buffer),"create storage buffer");
    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(object.device,object.buffer,&req);
    VkPhysicalDeviceMemoryProperties memory_props{};
    vkGetPhysicalDeviceMemoryProperties(selected,&memory_props);
    for(std::uint32_t i=0;i<memory_props.memoryTypeCount;i++)
        if((req.memoryTypeBits&(1u<<i))&&
           (memory_props.memoryTypes[i].propertyFlags&
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
            (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){
            memory_index=i;break;
        }
    if(memory_index==std::numeric_limits<std::uint32_t>::max()){
        std::cout<<"SKIP: No host coherent heap compatible with storage buffer\n";
        return 77;
    }
    VkMemoryAllocateInfo alloc{};
    alloc.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc.allocationSize=req.size;
    alloc.memoryTypeIndex=memory_index;
    check(vkAllocateMemory(object.device,&alloc,nullptr,&object.memory),"allocate buffer memory");
    check(vkBindBufferMemory(object.device,object.buffer,object.memory,0),"bind buffer memory");
    check(vkMapMemory(object.device,object.memory,0,bufferBytes,0,&object.mapped),"map storage buffer");
    auto* words = static_cast<std::uint32_t*>(object.mapped);
    if (partition)
        std::memcpy(words, input.data(), inputBytes);
    auto* result = words + input.size();
    for (std::size_t i = 0; i < reference.size(); ++i)
        result[i] = ~reference[i];  // An unwritten word must fail comparison.

    // The original shader writes binding 0. The partition shader reads binding 0 and writes binding 1.
    const std::uint32_t bindingCount = partition ? 2 : 1;
    std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
    for (std::uint32_t i = 0; i < bindingCount; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }
    VkDescriptorSetLayoutCreateInfo set_ci{};
    set_ci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_ci.bindingCount=bindingCount;
    set_ci.pBindings=bindings.data();
    check(vkCreateDescriptorSetLayout(object.device,&set_ci,nullptr,&object.set_layout),
          "create descriptor set layout");
    VkDescriptorPoolSize pool_size{};
    pool_size.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_size.descriptorCount=bindingCount;
    VkDescriptorPoolCreateInfo pool_ci{};
    pool_ci.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_ci.poolSizeCount=1;pool_ci.pPoolSizes=&pool_size;
    pool_ci.maxSets=1;
    check(vkCreateDescriptorPool(object.device,&pool_ci,nullptr,&object.descriptor_pool),
          "create descriptor pool");
    VkDescriptorSetAllocateInfo set_alloc{};
    set_alloc.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    set_alloc.descriptorPool=object.descriptor_pool;
    set_alloc.descriptorSetCount=1;
    set_alloc.pSetLayouts=&object.set_layout;
    VkDescriptorSet descriptor=VK_NULL_HANDLE;
    check(vkAllocateDescriptorSets(object.device,&set_alloc,&descriptor),"allocate descriptor");
    std::array<VkDescriptorBufferInfo, 2> buffers{};
    buffers[0] = {object.buffer, 0, inputBytes};
    buffers[bindingCount - 1] = {object.buffer, inputBytes, outputBytes};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::uint32_t i = 0; i < bindingCount; ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = descriptor;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i].pBufferInfo = &buffers[i];
    }
    vkUpdateDescriptorSets(object.device, bindingCount, writes.data(), 0, nullptr);
    VkPipelineLayoutCreateInfo layout_ci{};
    layout_ci.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_ci.setLayoutCount=1;layout_ci.pSetLayouts=&object.set_layout;
    check(vkCreatePipelineLayout(object.device,&layout_ci,nullptr,&object.pipeline_layout),
          "create pipeline layout");
    VkShaderModuleCreateInfo shader_ci{};
    shader_ci.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_ci.codeSize=spirv.size()*4;
    shader_ci.pCode=spirv.data();
    check(vkCreateShaderModule(object.device,&shader_ci,nullptr,&object.shader_module),
          "create SPIR-V module");
    VkComputePipelineCreateInfo pipeline_ci{};
    pipeline_ci.sType=VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_ci.layout=object.pipeline_layout;
    pipeline_ci.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipeline_ci.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_ci.stage.module=object.shader_module;
    pipeline_ci.stage.pName="main";
    check(vkCreateComputePipelines(object.device,VK_NULL_HANDLE,1,&pipeline_ci,
                                   nullptr,&object.pipeline),"create compute pipeline");
    VkCommandPoolCreateInfo cmdpool_ci{};
    cmdpool_ci.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cmdpool_ci.queueFamilyIndex=family;
    check(vkCreateCommandPool(object.device,&cmdpool_ci,nullptr,&object.command_pool),
          "create command pool");
    VkCommandBufferAllocateInfo cmdalloc{};
    cmdalloc.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdalloc.commandPool=object.command_pool;
    cmdalloc.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdalloc.commandBufferCount=1;
    VkCommandBuffer command=VK_NULL_HANDLE;
    check(vkAllocateCommandBuffers(object.device,&cmdalloc,&command),"allocate command buffer");
    VkCommandBufferBeginInfo begin{};
    begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    check(vkBeginCommandBuffer(command,&begin),"begin command buffer");
    vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,object.pipeline);
    vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,
                            object.pipeline_layout,0,1,&descriptor,0,nullptr);
    vkCmdDispatch(command,groups,1,1);
    VkBufferMemoryBarrier barrier{};
    barrier.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
    barrier.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer=object.buffer;barrier.offset=inputBytes;barrier.size=outputBytes;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&barrier,0,nullptr);
    check(vkEndCommandBuffer(command),"end command buffer");
    VkFenceCreateInfo fence_ci{};
    fence_ci.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    check(vkCreateFence(object.device,&fence_ci,nullptr,&object.fence),"create fence");
    VkSubmitInfo submit{};
    submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount=1;
    submit.pCommandBuffers=&command;
    check(vkQueueSubmit(queue,1,&submit,object.fence),"submit compute dispatch");
    check(vkWaitForFences(object.device,1,&object.fence,VK_TRUE,10000000000ull),
          "wait for compute fence");
    std::uint64_t checksum=0;
    for(std::size_t i=0;i<reference.size();i++){
        if(result[i]!=reference[i]){
            if (partition) {
                const auto lane = i / kPartitionWordsPerLane;
                std::cerr << "partition mismatch: group=" << lane / kLanesPerGroup
                          << " lane=" << lane % kLanesPerGroup << " field=" << i % kPartitionWordsPerLane
                          << " expected=" << reference[i] << " actual=" << result[i] << '\n';
                return 1;
            }
            throw std::runtime_error("compute readback mismatch at index "+std::to_string(i));
        }
        checksum+=result[i];
    }
    if (partition) {
        VkPhysicalDeviceSubgroupProperties subgroup{};
        subgroup.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        VkPhysicalDeviceProperties2 properties{};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &subgroup;
        const auto getProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(
            vkGetInstanceProcAddr(object.instance, "vkGetPhysicalDeviceProperties2"));
        if (!getProperties) throw std::runtime_error("Vulkan 1.1 device properties unavailable");
        getProperties(selected, &properties);
        std::cout << "{\"kind\":\"wave64-partition\",\"reported_subgroup_size\":" << subgroup.subgroupSize
                  << ",\"device_type\":" << props.deviceType << ",\"workgroups\":" << groups
                  << ",\"words_verified\":" << reference.size() << "}\n";
        return 0;
    }
    std::cout<<"{\"kind\":\"native-linux-elf-vulkan-compute-readback\","
             "\"vendor_id\":"<<props.vendorID<<","
             "\"device_id\":"<<props.deviceID<<","
             "\"device_type\":"<<static_cast<std::uint32_t>(props.deviceType)<<","
             "\"compute_family\":"<<family<<","
             "\"shader_invocations\":"<<kElements<<","
             "\"checksum_u64\":"<<checksum<<","
             "\"command_buffer_submitted\":true,"
             "\"fence_waited\":true,"
             "\"host_readback_verified\":true,"
             "\"hardware_gpu_verified\":false,"
             "\"graphics_raster_or_present_tested\":false,"
             "\"game_shaders_tested\":false}\n";
    return 0;
}
}
int main(int argc,char** argv){
    if (argc != 2 && argc != 4) {
        std::cerr << "Usage: linux_vulkan_compute_readback SHADER.spv [PARTITION_INPUT.u32 REFERENCE.u32]\n";
        return 2;
    }
    try {
        if (argc == 4)
            return test(argv[1], load_vectors(argv[2]), load_vectors(argv[3]));
        std::vector<std::uint32_t> reference(kElements);
        for (std::uint32_t i = 0; i < kElements; ++i) reference[i] = expected(i);
        return test(argv[1], {}, reference);
    }
    catch(const std::exception& ex){
        std::cerr<<"native Linux Vulkan compute FAILED: "<<ex.what()<<"\n";
        return 1;
    }
}
