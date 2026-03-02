#include "VkContext.h"

#include <array>
#include <cstring>
#include <iostream>
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
        if (properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
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

static bool createCommandBuffer() noexcept
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = ctx->commandPool;
    allocInfo.commandBufferCount = 1;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    if (VK_FAILED(vkAllocateCommandBuffers(ctx->device, &allocInfo, &ctx->commandBuffer)))
    {
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(ctx->commandBuffer, &beginInfo);

    return true;
}

static bool createDescriptorPool(const Args& args) noexcept
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSize.descriptorCount = 1000;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1000;

    return !VK_FAILED(vkCreateDescriptorPool(ctx->device, &poolInfo, nullptr, &ctx->descriptorPool));
}

static bool createDescriptorSetLayouts() noexcept
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

    if (VK_FAILED(vkCreateDescriptorSetLayout(ctx->device, &createInfo, nullptr, &ctx->descriptorSetLayout)))
    {
        return false;
    }
    return true;
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

    if (!createDescriptorSetLayouts())
    {
        std::cout << "Failed to create vulkan descriptor set layouts\n";
        return false;
    }

    if (!createCommandPool())
    {
        std::cout << "Failed to create vulkan command pool\n";
        return false;
    }

    if (!createCommandBuffer())
    {
        std::cout << "Failed to create vulkan command buffer\n";
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

[[nodiscard]] VkCommandBuffer beginCommandBuffer() noexcept
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = ctx->commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    if (VK_FAILED(vkAllocateCommandBuffers(ctx->device, &allocInfo, &commandBuffer)))
    {
        std::cout << "Failed to allocate vulkan command buffer\n";
        return nullptr;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (VK_FAILED(vkBeginCommandBuffer(commandBuffer, &beginInfo)))
    {
        std::cout << "Failed to begin vulkan command buffer\n";
        return nullptr;
    }

    return commandBuffer;
}

[[nodiscard]] bool endCommandBuffer(VkCommandBuffer commandBuffer,
                                    const std::vector<VkSemaphore>& waitSemaphores,
                                    const std::vector<VkPipelineStageFlags>& waitStages,
                                    VkSubmitInfo& submitInfo) noexcept
{
    if (VK_FAILED(vkEndCommandBuffer(commandBuffer)))
    {
        std::cout << "Failed to end vulkan command buffer\n";
        return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkSemaphore signalSemaphore{};
    if (VK_FAILED(vkCreateSemaphore(ctx->device, &semaphoreInfo, nullptr, &signalSemaphore)))
    {
        std::cout << "Failed to create vulkan semaphore\n";
        return false;
    }

    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.waitSemaphoreCount = waitSemaphores.size();
    submitInfo.pWaitDstStageMask = waitStages.data();
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &signalSemaphore;
    return true;
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