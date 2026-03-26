#include "VkContext.h"

#include "QLog.h"
#include "Utils.h"

#include <array>
#include <cstring>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint
{
std::unique_ptr<VkContext> ctx{};

#ifndef NDEBUG
std::array<const char*, 1> validationLayers = {"VK_LAYER_KHRONOS_validation"};

static VkDebugUtilsMessengerEXT debugMessenger{};

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* pUserData)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        qlog::debug("Validation layer: " + std::string(pCallbackData->pMessage));
    }

    return VK_FALSE;
}

static VkResult createDebugUtilsMessengerEXT(VkInstance instance,
                                             const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                             const VkAllocationCallbacks* pAllocator,
                                             VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void destroyDebugUtilsMessengerEXT(VkInstance instance,
                                          VkDebugUtilsMessengerEXT debugMessenger,
                                          const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

static bool checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto& validationLayer : validationLayers)
    {
        bool layerFound = false;

        for (const auto& availableLayer : availableLayers)
        {
            if (strcmp(validationLayer, availableLayer.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }
    return true;
}

void VkContext::setupDebugCallback() noexcept
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    if (VK_FAILED(createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger)))
    {
        fail("Failed to create vulkan debug utils messenger");
    }
}
#endif

void VkContext::createInstance() noexcept
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "flint";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "flintVK";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

#ifndef NDEBUG
    if (!checkValidationLayerSupport())
    {
        fail("Vulkan validation layers requested are not supported");
    }
    createInfo.enabledLayerCount = validationLayers.size();
    createInfo.ppEnabledLayerNames = validationLayers.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    populateDebugMessengerCreateInfo(debugCreateInfo);
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;

    createInfo.enabledExtensionCount = 1;
    const char* extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    createInfo.ppEnabledExtensionNames = &extension;
#endif

    if (VK_FAILED(vkCreateInstance(&createInfo, nullptr, &instance)))
    {
        fail("Failed to create vulkan instance");
    }
}

// TODO: better gpu picking
void VkContext::choosePhysicalDevice() noexcept
{
    uint32_t deviceCount{};
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        fail("Failed to find a vulkan physical device");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    for (const auto& device : devices)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            gpu = device;
            return;
        }
    }
    fail("Failed to find a suitable vulkan physical device");
}

uint8_t VkContext::chooseQueueFamily() noexcept
{
    uint32_t queueFamilyCount{};
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
    {
        return false;
    }
    std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, properties.data());
    for (int i = 0; i < queueFamilyCount; ++i)
    {
        if (properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            return i;
        }
    }
    return 255;
}

void VkContext::createLogicalDevice() noexcept
{
    queueFamilyIndex = chooseQueueFamily();

    float priority = 1.f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceTimelineSemaphoreFeatures features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    features.timelineSemaphore = true;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = nullptr;
    createInfo.enabledExtensionCount = 1;
    const char* extensions[] = {VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME};
    createInfo.ppEnabledExtensionNames = extensions;
#ifndef NDEBUG
    createInfo.enabledLayerCount = validationLayers.size();
    createInfo.ppEnabledLayerNames = validationLayers.data();
#endif
    createInfo.pNext = &features;

    if (VK_FAILED(vkCreateDevice(gpu, &createInfo, nullptr, &device)))
    {
        fail("Failed to create vulkan device");
    }

    vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
}

void VkContext::createCommandPool() noexcept
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;

    if (VK_FAILED(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool)))
    {
        fail("Failed to create vulkan command pool");
    }
}

void VkContext::createDescriptorPool() noexcept
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 1000;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1000;

    if (VK_FAILED(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool)))
    {
        fail("Failed to create vulkan descriptor pool");
    }
}

void VkContext::createDescriptorSetLayout() noexcept
{
    VkDescriptorSetLayoutBinding layoutBinding{};
    layoutBinding.binding = 0;
    layoutBinding.descriptorCount = 1;
    layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = 1;
    createInfo.pBindings = &layoutBinding;

    if (VK_FAILED(vkCreateDescriptorSetLayout(device, &createInfo, nullptr, &descriptorSetLayout)))
    {
        fail("Failed to create vulkan descriptor set layout");
    }
}

VkContext::VkContext() noexcept :
    instance(nullptr), gpu(nullptr), device(nullptr), queueFamilyIndex(255), queue(nullptr), commandPool(nullptr),
    descriptorPool(nullptr), descriptorSetLayout(nullptr)
{
    createInstance();
#ifndef NDEBUG
    setupDebugCallback();
#endif
    choosePhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createDescriptorPool();
    createDescriptorSetLayout();
}

VkContext::VkContext(VkContext&& other) noexcept :
    instance(other.instance), gpu(other.gpu), device(other.device), queueFamilyIndex(other.queueFamilyIndex),
    queue(other.queue), commandPool(other.commandPool), descriptorPool(other.descriptorPool),
    descriptorSetLayout(other.descriptorSetLayout)
{
    other.instance = nullptr;
    other.gpu = nullptr;
    other.device = nullptr;
    other.queueFamilyIndex = 255;
    other.queue = nullptr;
    other.commandPool = nullptr;
    other.descriptorPool = nullptr;
    other.descriptorSetLayout = nullptr;
}

void VkContext::operator=(VkContext&& other) noexcept
{
    instance = other.instance;
    gpu = other.gpu;
    device = other.device;
    queueFamilyIndex = other.queueFamilyIndex;
    queue = other.queue;
    commandPool = other.commandPool;
    descriptorPool = other.descriptorPool;
    descriptorSetLayout = other.descriptorSetLayout;

    other.instance = nullptr;
    other.gpu = nullptr;
    other.device = nullptr;
    other.queueFamilyIndex = 255;
    other.queue = nullptr;
    other.commandPool = nullptr;
    other.descriptorPool = nullptr;
    other.descriptorSetLayout = nullptr;
}

void initVk() noexcept
{
    ctx = std::make_unique<VkContext>();
}

void cleanupVk() noexcept
{
    ctx.reset(nullptr);
}

VkContext::~VkContext() noexcept
{
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
#ifndef NDEBUG
    destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
    vkDestroyInstance(instance, nullptr);
}

bool findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags, int& memoryType) noexcept
{
    VkPhysicalDeviceMemoryProperties memProperties{};
    vkGetPhysicalDeviceMemoryProperties(ctx->gpu, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
    {
        if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags)
        {
            memoryType = i;
            return true;
        }
    }
    return false;
}

VkImageMemoryBarrier createImageMemoryBarrier() noexcept
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    return barrier;
}
} // namespace flint