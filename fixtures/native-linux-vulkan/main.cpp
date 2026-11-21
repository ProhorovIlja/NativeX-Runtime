// Original native Linux x86-64 ELF Vulkan instance/device smoke test.
// Vulkan "physical device" may be software Mesa; not proof of a hardware GPU.
#include <vulkan/vulkan.h>
#include <cstdint>
#include <iostream>
#include <vector>
static int fail(const char* msg,VkResult r){
    std::cerr<<"native Linux Vulkan "<<msg<<" VkResult="<<static_cast<int>(r)<<'\n';
    return 1;
}
int main(){
#if !defined(__linux__) || !defined(__x86_64__)
    return fail("requires Linux x86-64",VK_ERROR_INITIALIZATION_FAILED);
#else
    std::uint32_t version=VK_API_VERSION_1_0;
    if(vkEnumerateInstanceVersion(&version)!=VK_SUCCESS)
        return fail("instance version",VK_ERROR_INITIALIZATION_FAILED);
    VkApplicationInfo app{};
    app.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName="Scarlett Original ELF Vulkan Probe";
    app.apiVersion=version>=VK_API_VERSION_1_1?VK_API_VERSION_1_1:VK_API_VERSION_1_0;
    VkInstanceCreateInfo ci{};
    ci.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo=&app;
    VkInstance instance=VK_NULL_HANDLE;
    VkResult result=vkCreateInstance(&ci,nullptr,&instance);
    if(result!=VK_SUCCESS)return fail("vkCreateInstance",result);
    std::uint32_t count=0;
    result=vkEnumeratePhysicalDevices(instance,&count,nullptr);
    if(result!=VK_SUCCESS){
        vkDestroyInstance(instance,nullptr);
        return fail("vkEnumeratePhysicalDevices",result);
    }
    if(count==0){
        vkDestroyInstance(instance,nullptr);
        std::cout<<"SKIP: Vulkan instance works; no Vulkan logical device possible\n";
        return 77;
    }
    std::vector<VkPhysicalDevice> devices(count);
    result=vkEnumeratePhysicalDevices(instance,&count,devices.data());
    if(result!=VK_SUCCESS||count==0){
        vkDestroyInstance(instance,nullptr);
        return fail("vkEnumeratePhysicalDevices second pass",result);
    }
    VkPhysicalDevice selected=VK_NULL_HANDLE;
    std::uint32_t family=0;
    VkPhysicalDeviceProperties props{};
    for(const auto device:devices){
        std::uint32_t n=0;
        vkGetPhysicalDeviceQueueFamilyProperties(device,&n,nullptr);
        if(!n)continue;
        std::vector<VkQueueFamilyProperties> queues(n);
        vkGetPhysicalDeviceQueueFamilyProperties(device,&n,queues.data());
        for(std::uint32_t i=0;i<n;i++){
            if(queues[i].queueCount && (queues[i].queueFlags&VK_QUEUE_GRAPHICS_BIT)){
                selected=device;family=i;
                vkGetPhysicalDeviceProperties(device,&props);
                break;
            }
        }
        if(selected!=VK_NULL_HANDLE)break;
    }
    if(selected==VK_NULL_HANDLE){
        vkDestroyInstance(instance,nullptr);
        std::cout<<"SKIP: Vulkan devices have no graphics queues\n";
        return 77;
    }
    const float priority=1.0f;
    VkDeviceQueueCreateInfo q{};
    q.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    q.queueFamilyIndex=family;q.queueCount=1;q.pQueuePriorities=&priority;
    VkDeviceCreateInfo d{};
    d.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    d.queueCreateInfoCount=1;d.pQueueCreateInfos=&q;
    VkDevice logical=VK_NULL_HANDLE;
    result=vkCreateDevice(selected,&d,nullptr,&logical);
    if(result!=VK_SUCCESS){
        vkDestroyInstance(instance,nullptr);
        return fail("vkCreateDevice",result);
    }
    VkQueue queue=VK_NULL_HANDLE;
    vkGetDeviceQueue(logical,family,0,&queue);
    if(queue==VK_NULL_HANDLE){
        vkDestroyDevice(logical,nullptr);vkDestroyInstance(instance,nullptr);
        return fail("vkGetDeviceQueue",VK_ERROR_INITIALIZATION_FAILED);
    }
    result=vkDeviceWaitIdle(logical);
    if(result!=VK_SUCCESS){
        vkDestroyDevice(logical,nullptr);vkDestroyInstance(instance,nullptr);
        return fail("vkDeviceWaitIdle",result);
    }
    std::cout<<"{\"kind\":\"native-linux-elf-vulkan-device-queue\","
             <<"\"vendor_id\":"<<props.vendorID<<","
             <<"\"device_id\":"<<props.deviceID<<","
             <<"\"device_type\":"<<static_cast<unsigned>(props.deviceType)<<","
             <<"\"api_version\":"<<props.apiVersion<<","
             <<"\"graphics_family\":"<<family<<","
             <<"\"vulkan_calls_executed\":true,"
             <<"\"hardware_gpu_verified\":false,"
             <<"\"render_or_present_tested\":false,"
             <<"\"commercial_game_tested\":false}\n";
    vkDestroyDevice(logical,nullptr);
    vkDestroyInstance(instance,nullptr);
    return 0;
#endif
}
