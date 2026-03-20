#pragma once

#include "cmdparser.hpp"

#include <memory>
#include <vulkan/vulkan.h>

namespace flint
{
#define VK_FAILED(x) (x != VK_SUCCESS)

struct VkContext final
{
    VkContext(const cli::Parser&) noexcept;

    VkContext(VkContext&) = delete;

    VkContext(VkContext&&) noexcept;

    void operator=(VkContext&) = delete;

    void operator=(VkContext&&) noexcept;

    ~VkContext() noexcept;

    VkInstance instance{};
    VkPhysicalDevice gpu{};
    VkDevice device{};
    uint8_t queueFamilyIndex = 255;
    VkQueue queue{};
    VkCommandPool commandPool{};
    VkDescriptorPool descriptorPool{};
    VkDescriptorSetLayout descriptorSetLayout{};

private:
    void createInstance() noexcept;

#ifndef NDEBUG
    void setupDebugCallback() noexcept;
#endif

    void choosePhysicalDevice() noexcept;

    uint8_t chooseQueueFamily() noexcept;

    void createLogicalDevice() noexcept;

    void createDescriptorPool(const cli::Parser&) noexcept;

    void createDescriptorSetLayout() noexcept;

    void createCommandPool() noexcept;
};

void initVk(const cli::Parser&) noexcept;
void cleanupVk() noexcept;

bool findMemoryType(uint32_t, VkMemoryPropertyFlags, int&) noexcept;

VkImageMemoryBarrier createImageMemoryBarrier() noexcept;

extern std::unique_ptr<VkContext> ctx;
} // namespace flint