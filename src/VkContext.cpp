#include "VkContext.h"

#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace flint::vulkan
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
        std::cout << "Validation layer: " << pCallbackData->pMessage << std::endl;
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
        func(instance, debugMessenger, pAllocator);
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

static bool setupDebugCallback()
{
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    return !VK_FAILED(createDebugUtilsMessengerEXT(ctx->instance, &createInfo, nullptr, &debugMessenger));
}
#endif

static bool createInstance()
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
        std::cout << "Vulkan validation layers requested are not supported\n";
        return false;
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
    return !VK_FAILED(vkCreateInstance(&createInfo, nullptr, &ctx->instance));
}

// TODO: better gpu picking
static bool choosePhysicalDevice() noexcept
{
    uint32_t deviceCount{};
    vkEnumeratePhysicalDevices(ctx->instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(ctx->instance, &deviceCount, devices.data());
    for (const auto& device : devices)
    {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(device, &properties);
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            ctx->gpu = device;
            return true;
        }
    }
    return false;
}

static uint8_t chooseQueueFamily() noexcept
{
    uint32_t queueFamilyCount{};
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->gpu, &queueFamilyCount, nullptr);
    if (queueFamilyCount == 0)
    {
        return false;
    }
    std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->gpu, &queueFamilyCount, properties.data());
    for (int i = 0; i < queueFamilyCount; ++i)
    {
        if (properties[i].queueFlags * VK_QUEUE_COMPUTE_BIT)
        {
            return i;
        }
    }
    return 255;
}

static bool createLogicalDevice() noexcept
{
    ctx->queueFamilyIndex = chooseQueueFamily();

    float priority = 1.f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = ctx->queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priority;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pEnabledFeatures = nullptr;
#ifndef NDEBUG
    createInfo.enabledLayerCount = validationLayers.size();
    createInfo.ppEnabledLayerNames = validationLayers.data();
#endif

    if (VK_FAILED(vkCreateDevice(ctx->gpu, &createInfo, nullptr, &ctx->device)))
    {
        return false;
    }

    vkGetDeviceQueue(ctx->device, ctx->queueFamilyIndex, 0, &ctx->queue);
    return true;
}

static bool createCommandPool() noexcept
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = ctx->queueFamilyIndex;

    return !VK_FAILED(vkCreateCommandPool(ctx->device, &poolInfo, nullptr, &ctx->commandPool));
}

static bool createDescriptorPool(const Args& args) noexcept
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2;

    return !VK_FAILED(vkCreateDescriptorPool(ctx->device, &poolInfo, nullptr, &ctx->descriptorPool));
}

static bool createDescriptorSetLayout() noexcept
{
    std::array<VkDescriptorSetLayoutBinding, 2> layoutBindings{};
    for (int i = 0; i < layoutBindings.size(); ++i)
    {
        layoutBindings[i].binding = i;
        layoutBindings[i].descriptorCount = 1;
        layoutBindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        layoutBindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    createInfo.bindingCount = layoutBindings.size();
    createInfo.pBindings = layoutBindings.data();

    return !VK_FAILED(vkCreateDescriptorSetLayout(ctx->device, &createInfo, nullptr, &ctx->descriptorSetLayout));
}

static bool initContext(const Args& args)
{
    ctx = std::make_unique<VkContext>();
    if (!createInstance())
    {
        std::cout << "Failed to create vulkan instance\n";
        return false;
    }

#ifndef NDEBUG
    setupDebugCallback();
#endif

    if (!choosePhysicalDevice())
    {
        std::cout << "Failed to find suitable vulkan physical device\n";
        return false;
    }

    if (!createLogicalDevice())
    {
        std::cout << "Failed to create vulkan logical device\n";
        return false;
    }

    if (!createDescriptorPool(args))
    {
        std::cout << "Failed to create vulkan descriptor pool\n";
        return false;
    }

    if (!createDescriptorSetLayout())
    {
        std::cout << "Failed to create vulkan descriptor set layout\n";
        return false;
    }

    if (!createCommandPool())
    {
        std::cout << "Failed to create vulkan command pool\n";
        return false;
    }

    return true;
}

bool init(const Args& args) noexcept
{
    if (!initContext(args))
    {
        std::cout << "Failed to create vulkan context\n";
        return false;
    }
    return true;
}

void cleanup() noexcept
{
#ifndef NDEBUG
    destroyDebugUtilsMessengerEXT(ctx->instance, debugMessenger, nullptr);
#endif
    vkDestroyCommandPool(ctx->device, ctx->commandPool, nullptr);

    vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptorSetLayout, nullptr);

    vkDestroyDescriptorPool(ctx->device, ctx->descriptorPool, nullptr);

    vkDestroyDevice(ctx->device, nullptr);

    vkDestroyInstance(ctx->instance, nullptr);
}
} // namespace flint::vulkan